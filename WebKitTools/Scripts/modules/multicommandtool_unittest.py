# Copyright (c) 2009, Google Inc. All rights reserved.
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

import sys
import unittest
from multicommandtool import MultiCommandTool, Command
from modules.outputcapture import OutputCapture

from optparse import make_option

class TrivialCommand(Command):
    name = "trivial"
    show_in_main_help = True
    def __init__(self, **kwargs):
        Command.__init__(self, "help text", **kwargs)

    def execute(self, options, args, tool):
        pass

class UncommonCommand(TrivialCommand):
    name = "uncommon"
    show_in_main_help = False

class CommandTest(unittest.TestCase):
    def test_name_with_arguments(self):
        command_with_args = TrivialCommand(argument_names="ARG1 ARG2")
        self.assertEqual(command_with_args.name_with_arguments(), "trivial ARG1 ARG2")

        command_with_args = TrivialCommand(options=[make_option("--my_option")])
        self.assertEqual(command_with_args.name_with_arguments(), "trivial [options]")

    def test_parse_required_arguments(self):
        self.assertEqual(Command._parse_required_arguments("ARG1 ARG2"), ["ARG1", "ARG2"])
        self.assertEqual(Command._parse_required_arguments("[ARG1] [ARG2]"), [])
        self.assertEqual(Command._parse_required_arguments("[ARG1] ARG2"), ["ARG2"])
        # Note: We might make our arg parsing smarter in the future and allow this type of arguments string.
        self.assertRaises(Exception, Command._parse_required_arguments, "[ARG1 ARG2]")

    def test_required_arguments(self):
        two_required_arguments = TrivialCommand(argument_names="ARG1 ARG2 [ARG3]")
        expected_missing_args_error = "2 arguments required, 1 argument provided.  Provided: 'foo'  Required: ARG1 ARG2\nSee 'trivial-tool help trivial' for usage.\n"
        exit_code = OutputCapture().assert_outputs(self, two_required_arguments.check_arguments_and_execute, [["foo"], TrivialTool()], expected_stderr=expected_missing_args_error)
        self.assertEqual(exit_code, 1)


class TrivialTool(MultiCommandTool):
    def __init__(self, commands=None):
        MultiCommandTool.__init__(self, name="trivial-tool", commands=commands)

    def path():
        return __file__

    def should_execute_command(self, command):
        return (True, None)


class MultiCommandToolTest(unittest.TestCase):
    def _assert_split(self, args, expected_split):
        self.assertEqual(MultiCommandTool._split_args(args), expected_split)

    def test_split_args(self):
        # MultiCommandToolTest._split_args returns: (global_args, command, command_args)
        full_args = ["--global-option", "command", "--option", "arg"]
        full_args_expected = (["--global-option"], "command", ["--option", "arg"])
        self._assert_split(full_args, full_args_expected)

        full_args = []
        full_args_expected = ([], None, [])
        self._assert_split(full_args, full_args_expected)

        full_args = ["command", "arg"]
        full_args_expected = ([], "command", ["arg"])
        self._assert_split(full_args, full_args_expected)

    def test_command_by_name(self):
        # This also tests Command auto-discovery.
        tool = TrivialTool()
        self.assertEqual(tool.command_by_name("trivial").name, "trivial")
        self.assertEqual(tool.command_by_name("bar"), None)

    def _assert_tool_main_outputs(self, tool, main_args, expected_stdout, expected_stderr = "", expected_exit_code=0):
        exit_code = OutputCapture().assert_outputs(self, tool.main, [main_args], expected_stdout=expected_stdout, expected_stderr=expected_stderr)
        self.assertEqual(exit_code, expected_exit_code)

    def test_global_help(self):
        tool = TrivialTool(commands=[TrivialCommand(), UncommonCommand()])
        expected_common_commands_help = """Usage: trivial-tool [options] COMMAND [ARGS]

Options:
  -h, --help  show this help message and exit

Common trivial-tool commands:
   trivial   help text

See 'trivial-tool help --all-commands' to list all commands.
See 'trivial-tool help COMMAND' for more information on a specific command.

"""
        self._assert_tool_main_outputs(tool, ["tool", "help"], expected_common_commands_help)
        expected_all_commands_help = """Usage: trivial-tool [options] COMMAND [ARGS]

Options:
  -h, --help  show this help message and exit

All trivial-tool commands:
   help       Display information about this program or its subcommands
   trivial    help text
   uncommon   help text

See 'trivial-tool help --all-commands' to list all commands.
See 'trivial-tool help COMMAND' for more information on a specific command.

"""
        self._assert_tool_main_outputs(tool, ["tool", "help", "--all-commands"], expected_all_commands_help)

    def test_command_help(self):
        command_with_options = TrivialCommand(options=[make_option("--my_option")])
        tool = TrivialTool(commands=[command_with_options])
        expected_subcommand_help = "trivial [options]   help text\nOptions:\n  --my_option=MY_OPTION\n\n"
        self._assert_tool_main_outputs(tool, ["tool", "help", "trivial"], expected_subcommand_help)


if __name__ == "__main__":
    unittest.main()
