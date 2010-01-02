#!/usr/bin/perl -wT
use strict;
use CGI;

my $cgi = new CGI;

print "Content-Type: text/html; charset=UTF-8\n\n";

print "<!DOCTYPE html>\n";
print "<html>\n";
print "<body>\n";
print $cgi->param('q');
if ($cgi->param('notifyDone')) {
    print "<script>\n";
    print "if (window.layoutTestController)\n";
    print "    layoutTestController.notifyDone();\n";
    print "</script>\n";
}
print "</body>\n";
print "</html>\n";
