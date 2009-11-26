#!/usr/bin/env python
#
# Copyright 2009, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


"""Standalone Web Socket server.

Use this server to run mod_pywebsocket without Apache HTTP Server.

Usage:
    python standalone.py [-p <ws_port>] [-w <websock_handlers>]
                         [-s <scan_dir>]
                         [-d <document_root>]
                         ... for other options, see _main below ...

<ws_port> is the port number to use for ws:// connection.

<document_root> is the path to the root directory of HTML files.

<websock_handlers> is the path to the root directory of Web Socket handlers.
See __init__.py for details of <websock_handlers> and how to write Web Socket
handlers. If this path is relative, <document_root> is used as the base.

<scan_dir> is a path under the root directory. If specified, only the handlers
under scan_dir are scanned. This is useful in saving scan time.

Note:
This server is derived from SocketServer.ThreadingMixIn. Hence a thread is
used for each request.
"""

import BaseHTTPServer
import SimpleHTTPServer
import SocketServer
import logging
import logging.handlers
import optparse
import os
import socket
import sys

_HAS_OPEN_SSL = False
try:
    import OpenSSL.SSL
    _HAS_OPEN_SSL = True
except ImportError:
    pass

import dispatch
import handshake


_LOG_LEVELS = {
    'debug': logging.DEBUG,
    'info': logging.INFO,
    'warn': logging.WARN,
    'error': logging.ERROR,
    'critical': logging.CRITICAL};

_DEFAULT_LOG_MAX_BYTES = 1024 * 256
_DEFAULT_LOG_BACKUP_COUNT = 5


def _print_warnings_if_any(dispatcher):
    warnings = dispatcher.source_warnings()
    if warnings:
        for warning in warnings:
            logging.warning('mod_pywebsocket: %s' % warning)


class _StandaloneConnection(object):
    """Mimic mod_python mp_conn."""

    def __init__(self, request_handler):
        """Construct an instance.

        Args:
            request_handler: A WebSocketRequestHandler instance.
        """
        self._request_handler = request_handler

    def get_local_addr(self):
        """Getter to mimic mp_conn.local_addr."""
        return (self._request_handler.server.server_name,
                self._request_handler.server.server_port)
    local_addr = property(get_local_addr)

    def write(self, data):
        """Mimic mp_conn.write()."""
        return self._request_handler.wfile.write(data)

    def read(self, length):
        """Mimic mp_conn.read()."""
        return self._request_handler.rfile.read(length)


class _StandaloneRequest(object):
    """Mimic mod_python request."""

    def __init__(self, request_handler, use_tls):
        """Construct an instance.

        Args:
            request_handler: A WebSocketRequestHandler instance.
        """
        self._request_handler = request_handler
        self.connection = _StandaloneConnection(request_handler)
        self._use_tls = use_tls

    def get_uri(self):
        """Getter to mimic request.uri."""
        return self._request_handler.path
    uri = property(get_uri)

    def get_headers_in(self):
        """Getter to mimic request.headers_in."""
        return self._request_handler.headers
    headers_in = property(get_headers_in)

    def is_https(self):
        """Mimic request.is_https()."""
        return self._use_tls


class WebSocketServer(SocketServer.ThreadingMixIn, BaseHTTPServer.HTTPServer):
    """HTTPServer specialized for Web Socket."""

    SocketServer.ThreadingMixIn.daemon_threads = True

    def __init__(self, server_address, RequestHandlerClass):
        """Override SocketServer.BaseServer.__init__."""

        SocketServer.BaseServer.__init__(
                self, server_address, RequestHandlerClass)
        self.socket = self._create_socket()
        self.server_bind()
        self.server_activate()

    def _create_socket(self):
        socket_ = socket.socket(self.address_family, self.socket_type)
        if WebSocketServer.options.use_tls:
            ctx = OpenSSL.SSL.Context(OpenSSL.SSL.SSLv23_METHOD)
            ctx.use_privatekey_file(WebSocketServer.options.private_key)
            ctx.use_certificate_file(WebSocketServer.options.certificate)
            socket_ = OpenSSL.SSL.Connection(ctx, socket_)
        return socket_


class WebSocketRequestHandler(SimpleHTTPServer.SimpleHTTPRequestHandler):
    """SimpleHTTPRequestHandler specialized for Web Socket."""

    def setup(self):
        """Override SocketServer.StreamRequestHandler.setup."""

        self.connection = self.request
        self.rfile = socket._fileobject(self.request, 'rb', self.rbufsize)
        self.wfile = socket._fileobject(self.request, 'wb', self.wbufsize)

    def __init__(self, *args, **keywords):
        self._request = _StandaloneRequest(
                self, WebSocketRequestHandler.options.use_tls)
        self._dispatcher = WebSocketRequestHandler.options.dispatcher
        self._print_warnings_if_any()
        self._handshaker = handshake.Handshaker(self._request,
                                                self._dispatcher)
        SimpleHTTPServer.SimpleHTTPRequestHandler.__init__(
                self, *args, **keywords)

    def _print_warnings_if_any(self):
        warnings = self._dispatcher.source_warnings()
        if warnings:
            for warning in warnings:
                logging.warning('mod_pywebsocket: %s' % warning)

    def parse_request(self):
        """Override BaseHTTPServer.BaseHTTPRequestHandler.parse_request.

        Return True to continue processing for HTTP(S), False otherwise.
        """
        result = SimpleHTTPServer.SimpleHTTPRequestHandler.parse_request(self)
        if result:
            try:
                self._handshaker.do_handshake()
                self._dispatcher.transfer_data(self._request)
                return False
            except handshake.HandshakeError, e:
                # Handshake for ws(s) failed. Assume http(s).
                logging.info('mod_pywebsocket: %s' % e)
                return True
            except dispatch.DispatchError, e:
                logging.warning('mod_pywebsocket: %s' % e)
                return False
        return result

    def log_request(self, code='-', size='-'):
        """Override BaseHTTPServer.log_request."""

        logging.info('"%s" %s %s',
                     self.requestline, str(code), str(size))

    def log_error(self, *args):
        """Override BaseHTTPServer.log_error."""

        # Despite the name, this method is for warnings than for errors.
        # For example, HTTP status code is logged by this method.
        logging.warn('%s - %s' % (self.address_string(), (args[0] % args[1:])))


def _configure_logging(options):
    logger = logging.getLogger()
    logger.setLevel(_LOG_LEVELS[options.log_level])
    if options.log_file:
        handler = logging.handlers.RotatingFileHandler(
                options.log_file, 'a', options.log_max, options.log_count)
    else:
        handler = logging.StreamHandler()
    formatter = logging.Formatter(
            "[%(asctime)s] [%(levelname)s] %(name)s: %(message)s")
    handler.setFormatter(formatter)
    logger.addHandler(handler)


def _main():
    parser = optparse.OptionParser()
    parser.add_option('-p', '--port', dest='port', type='int',
                      default=handshake._DEFAULT_WEB_SOCKET_PORT,
                      help='port to listen to')
    parser.add_option('-w', '--websock_handlers', dest='websock_handlers',
                      default='.',
                      help='Web Socket handlers root directory.')
    parser.add_option('-s', '--scan_dir', dest='scan_dir',
                      default=None,
                      help=('Web Socket handlers scan directory. '
                            'Must be a directory under websock_handlers.'))
    parser.add_option('-d', '--document_root', dest='document_root',
                      default='.',
                      help='Document root directory.')
    parser.add_option('-t', '--tls', dest='use_tls', action='store_true',
                      default=False, help='use TLS (wss://)')
    parser.add_option('-k', '--private_key', dest='private_key',
                      default='', help='TLS private key file.')
    parser.add_option('-c', '--certificate', dest='certificate',
                      default='', help='TLS certificate file.')
    parser.add_option('-l', '--log_file', dest='log_file',
                      default='', help='Log file.')
    parser.add_option('--log_level', type='choice', dest='log_level',
                      default='warn',
                      choices=['debug', 'info', 'warn', 'error', 'critical'],
                      help='Log level.')
    parser.add_option('--log_max', dest='log_max', type='int',
                      default=_DEFAULT_LOG_MAX_BYTES,
                      help='Log maximum bytes')
    parser.add_option('--log_count', dest='log_count', type='int',
                      default=_DEFAULT_LOG_BACKUP_COUNT,
                      help='Log backup count')
    options = parser.parse_args()[0]

    os.chdir(options.document_root)

    _configure_logging(options)

    if options.use_tls:
        if not _HAS_OPEN_SSL:
            logging.critical('To use TLS, install pyOpenSSL.')
            sys.exit(1)
        if not options.private_key or not options.certificate:
            logging.critical(
                    'To use TLS, specify private_key and certificate.')
            sys.exit(1)

    if not options.scan_dir:
        options.scan_dir = options.websock_handlers

    try:
        # Share a Dispatcher among request handlers to save time for
        # instantiation.  Dispatcher can be shared because it is thread-safe.
        options.dispatcher = dispatch.Dispatcher(options.websock_handlers,
                                                 options.scan_dir)
        _print_warnings_if_any(options.dispatcher)

        WebSocketRequestHandler.options = options
        WebSocketServer.options = options

        server = WebSocketServer(('', options.port), WebSocketRequestHandler)
        server.serve_forever()
    except Exception, e:
        logging.critical(str(e))
        sys.exit(1)


if __name__ == '__main__':
    _main()


# vi:sts=4 sw=4 et
