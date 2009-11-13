# Copyright (C) 2007, 2008, 2009 Apple Inc.  All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer. 
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution. 
# 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
#     its contributors may be used to endorse or promote products derived
#     from this software without specific prior written permission. 
#
# THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Module to share code to work with various version control systems.
package VCSUtils;

use strict;
use warnings;

use Cwd qw();  # "qw()" prevents warnings about redefining getcwd() with "use POSIX;"
use File::Basename;
use File::Spec;

BEGIN {
    use Exporter   ();
    our ($VERSION, @ISA, @EXPORT, @EXPORT_OK, %EXPORT_TAGS);
    $VERSION     = 1.00;
    @ISA         = qw(Exporter);
    @EXPORT      = qw(
        &canonicalizePath
        &changeLogEmailAddress
        &changeLogName
        &chdirReturningRelativePath
        &decodeGitBinaryPatch
        &determineSVNRoot
        &determineVCSRoot
        &fixChangeLogPatch
        &gitBranch
        &gitdiff2svndiff
        &isGit
        &isGitBranchBuild
        &isGitDirectory
        &isSVN
        &isSVNDirectory
        &isSVNVersion16OrNewer
        &makeFilePathRelative
        &normalizePath
        &pathRelativeToSVNRepositoryRootForPath
        &svnRevisionForDirectory
        &svnStatus
    );
    %EXPORT_TAGS = ( );
    @EXPORT_OK   = ();
}

our @EXPORT_OK;

my $gitBranch;
my $gitRoot;
my $isGit;
my $isGitBranchBuild;
my $isSVN;
my $svnVersion;

sub isGitDirectory($)
{
    my ($dir) = @_;
    return system("cd $dir && git rev-parse > " . File::Spec->devnull() . " 2>&1") == 0;
}

sub isGit()
{
    return $isGit if defined $isGit;

    $isGit = isGitDirectory(".");
    return $isGit;
}

sub gitBranch()
{
    unless (defined $gitBranch) {
        chomp($gitBranch = `git symbolic-ref -q HEAD`);
        $gitBranch = "" if main::exitStatus($?); # FIXME: exitStatus is defined in webkitdirs.pm
        $gitBranch =~ s#^refs/heads/##;
        $gitBranch = "" if $gitBranch eq "master";
    }

    return $gitBranch;
}

sub isGitBranchBuild()
{
    my $branch = gitBranch();
    chomp(my $override = `git config --bool branch.$branch.webKitBranchBuild`);
    return 1 if $override eq "true";
    return 0 if $override eq "false";

    unless (defined $isGitBranchBuild) {
        chomp(my $gitBranchBuild = `git config --bool core.webKitBranchBuild`);
        $isGitBranchBuild = $gitBranchBuild eq "true";
    }

    return $isGitBranchBuild;
}

sub isSVNDirectory($)
{
    my ($dir) = @_;

    return -d File::Spec->catdir($dir, ".svn");
}

sub isSVN()
{
    return $isSVN if defined $isSVN;

    $isSVN = isSVNDirectory(".");
    return $isSVN;
}

sub svnVersion()
{
    return $svnVersion if defined $svnVersion;

    if (!isSVN()) {
        $svnVersion = 0;
    } else {
        chomp($svnVersion = `svn --version --quiet`);
    }
    return $svnVersion;
}

sub isSVNVersion16OrNewer()
{
    my $version = svnVersion();
    return eval "v$version" ge v1.6;
}

sub chdirReturningRelativePath($)
{
    my ($directory) = @_;
    my $previousDirectory = Cwd::getcwd();
    chdir $directory;
    my $newDirectory = Cwd::getcwd();
    return "." if $newDirectory eq $previousDirectory;
    return File::Spec->abs2rel($previousDirectory, $newDirectory);
}

sub determineGitRoot()
{
    chomp(my $gitDir = `git rev-parse --git-dir`);
    return dirname($gitDir);
}

sub determineSVNRoot()
{
    my $last = '';
    my $path = '.';
    my $parent = '..';
    my $repositoryRoot;
    my $repositoryUUID;
    while (1) {
        my $thisRoot;
        my $thisUUID;
        # Ignore error messages in case we've run past the root of the checkout.
        open INFO, "svn info '$path' 2> " . File::Spec->devnull() . " |" or die;
        while (<INFO>) {
            if (/^Repository Root: (.+)/) {
                $thisRoot = $1;
            }
            if (/^Repository UUID: (.+)/) {
                $thisUUID = $1;
            }
            if ($thisRoot && $thisUUID) {
                local $/ = undef;
                <INFO>; # Consume the rest of the input.
            }
        }
        close INFO;

        # It's possible (e.g. for developers of some ports) to have a WebKit
        # checkout in a subdirectory of another checkout.  So abort if the
        # repository root or the repository UUID suddenly changes.
        last if !$thisUUID;
        $repositoryUUID = $thisUUID if !$repositoryUUID;
        last if $thisUUID ne $repositoryUUID;

        last if !$thisRoot;
        $repositoryRoot = $thisRoot if !$repositoryRoot;
        last if $thisRoot ne $repositoryRoot;

        $last = $path;
        $path = File::Spec->catdir($parent, $path);
    }

    return File::Spec->rel2abs($last);
}

sub determineVCSRoot()
{
    if (isGit()) {
        return determineGitRoot();
    }

    if (!isSVN()) {
        # Some users have a workflow where svn-create-patch, svn-apply and
        # svn-unapply are used outside of multiple svn working directores,
        # so warn the user and assume Subversion is being used in this case.
        warn "Unable to determine VCS root; assuming Subversion";
        $isSVN = 1;
    }

    return determineSVNRoot();
}

sub svnRevisionForDirectory($)
{
    my ($dir) = @_;
    my $revision;

    if (isSVNDirectory($dir)) {
        my $svnInfo = `LC_ALL=C svn info $dir | grep Revision:`;
        ($revision) = ($svnInfo =~ m/Revision: (\d+).*/g);
    } elsif (isGitDirectory($dir)) {
        my $gitLog = `cd $dir && LC_ALL=C git log --grep='git-svn-id: ' -n 1 | grep git-svn-id:`;
        ($revision) = ($gitLog =~ m/ +git-svn-id: .+@(\d+) /g);
    }
    die "Unable to determine current SVN revision in $dir" unless (defined $revision);
    return $revision;
}

sub pathRelativeToSVNRepositoryRootForPath($)
{
    my ($file) = @_;
    my $relativePath = File::Spec->abs2rel($file);

    my $svnInfo;
    if (isSVN()) {
        $svnInfo = `LC_ALL=C svn info $relativePath`;
    } elsif (isGit()) {
        $svnInfo = `LC_ALL=C git svn info $relativePath`;
    }

    $svnInfo =~ /.*^URL: (.*?)$/m;
    my $svnURL = $1;

    $svnInfo =~ /.*^Repository Root: (.*?)$/m;
    my $repositoryRoot = $1;

    $svnURL =~ s/$repositoryRoot\///;
    return $svnURL;
}

sub makeFilePathRelative($)
{
    my ($path) = @_;
    return $path unless isGit();

    unless (defined $gitRoot) {
        chomp($gitRoot = `git rev-parse --show-cdup`);
    }
    return $gitRoot . $path;
}

sub normalizePath($)
{
    my ($path) = @_;
    $path =~ s/\\/\//g;
    return $path;
}

sub canonicalizePath($)
{
    my ($file) = @_;

    # Remove extra slashes and '.' directories in path
    $file = File::Spec->canonpath($file);

    # Remove '..' directories in path
    my @dirs = ();
    foreach my $dir (File::Spec->splitdir($file)) {
        if ($dir eq '..' && $#dirs >= 0 && $dirs[$#dirs] ne '..') {
            pop(@dirs);
        } else {
            push(@dirs, $dir);
        }
    }
    return ($#dirs >= 0) ? File::Spec->catdir(@dirs) : ".";
}

sub removeEOL($)
{
    my ($line) = @_;

    $line =~ s/[\r\n]+$//g;
    return $line;
}

sub svnStatus($)
{
    my ($fullPath) = @_;
    my $svnStatus;
    open SVN, "svn status --non-interactive --non-recursive '$fullPath' |" or die;
    if (-d $fullPath) {
        # When running "svn stat" on a directory, we can't assume that only one
        # status will be returned (since any files with a status below the
        # directory will be returned), and we can't assume that the directory will
        # be first (since any files with unknown status will be listed first).
        my $normalizedFullPath = File::Spec->catdir(File::Spec->splitdir($fullPath));
        while (<SVN>) {
            # Input may use a different EOL sequence than $/, so avoid chomp.
            $_ = removeEOL($_);
            my $normalizedStatPath = File::Spec->catdir(File::Spec->splitdir(substr($_, 7)));
            if ($normalizedFullPath eq $normalizedStatPath) {
                $svnStatus = "$_\n";
                last;
            }
        }
        # Read the rest of the svn command output to avoid a broken pipe warning.
        local $/ = undef;
        <SVN>;
    }
    else {
        # Files will have only one status returned.
        $svnStatus = removeEOL(<SVN>) . "\n";
    }
    close SVN;
    return $svnStatus;
}

sub gitdiff2svndiff($)
{
    $_ = shift @_;
    if (m#^diff --git a/(.+) b/(.+)#) {
        return "Index: $1";
    } elsif (m#^index [0-9a-f]{7}\.\.[0-9a-f]{7} [0-9]{6}#) {
        return "===================================================================";
    } elsif (m#^--- a/(.+)#) {
        return "--- $1";
    } elsif (m#^\+\+\+ b/(.+)#) {
        return "+++ $1";
    }
    return $_;
}

# The diff(1) command is greedy when matching lines, so a new ChangeLog entry will
# have lines of context at the top of a patch when the existing entry has the same
# date and author as the new entry.  Alter the ChangeLog patch so
# that the added lines ("+") in the patch always start at the beginning of the
# patch and there are no initial lines of context.
sub fixChangeLogPatch($)
{
    my $patch = shift; # $patch will only contain patch fragments for ChangeLog.

    $patch =~ /(\r?\n)/;
    my $lineEnding = $1;
    my @patchLines = split(/$lineEnding/, $patch);

    # e.g. 2009-06-03  Eric Seidel  <eric@webkit.org>
    my $dateLineRegexpString = '^\+(\d{4}-\d{2}-\d{2})' # Consume the leading '+' and the date.
                             . '\s+(.+)\s+' # Consume the name.
                             . '<([^<>]+)>$'; # And finally the email address.

    # Figure out where the patch contents start and stop.
    my $patchHeaderIndex;
    my $firstContentIndex;
    my $trailingContextIndex;
    my $dateIndex;
    my $patchEndIndex = scalar(@patchLines);
    for (my $index = 0; $index < @patchLines; ++$index) {
        my $line = $patchLines[$index];
        if ($line =~ /^\@\@ -\d+,\d+ \+\d+,\d+ \@\@$/) { # e.g. @@ -1,5 +1,18 @@
            if ($patchHeaderIndex) {
                $patchEndIndex = $index; # We only bother to fix up the first patch fragment.
                last;
            }
            $patchHeaderIndex = $index;
        }
        $firstContentIndex = $index if ($patchHeaderIndex && !$firstContentIndex && $line =~ /^\+[^+]/); # Only match after finding patchHeaderIndex, otherwise we'd match "+++".
        $dateIndex = $index if ($line =~ /$dateLineRegexpString/);
        $trailingContextIndex = $index if ($firstContentIndex && !$trailingContextIndex && $line =~ /^ /);
    }
    my $contentLineCount = $trailingContextIndex - $firstContentIndex;
    my $trailingContextLineCount = $patchEndIndex - $trailingContextIndex;

    # If we didn't find a date line in the content then this is not a patch we should try and fix.
    return $patch if (!$dateIndex);

    # We only need to do anything if the date line is not the first content line.
    return $patch if ($dateIndex == $firstContentIndex);

    # Write the new patch.
    my $totalNewContentLines = $contentLineCount + $trailingContextLineCount;
    $patchLines[$patchHeaderIndex] = "@@ -1,$trailingContextLineCount +1,$totalNewContentLines @@"; # Write a new header.
    my @repeatedLines = splice(@patchLines, $dateIndex, $trailingContextIndex - $dateIndex); # The date line and all the content after it that diff saw as repeated.
    splice(@patchLines, $firstContentIndex, 0, @repeatedLines); # Move the repeated content to the top.
    foreach my $line (@repeatedLines) {
        $line =~ s/^\+/ /;
    }
    splice(@patchLines, $trailingContextIndex, $patchEndIndex, @repeatedLines); # Replace trailing context with the repeated content.
    splice(@patchLines, $patchHeaderIndex + 1, $firstContentIndex - $patchHeaderIndex - 1); # Remove any leading context.

    return join($lineEnding, @patchLines) . "\n"; # patch(1) expects an extra trailing newline.
}

sub gitConfig($)
{
    return unless $isGit;

    my ($config) = @_;

    my $result = `git config $config`;
    if (($? >> 8)) {
        $result = `git repo-config $config`;
    }
    chomp $result;
    return $result;
}

sub changeLogNameError($)
{
    my ($message) = @_;
    print STDERR "$message\nEither:\n";
    print STDERR "  set CHANGE_LOG_NAME in your environment\n";
    print STDERR "  OR pass --name= on the command line\n";
    print STDERR "  OR set REAL_NAME in your environment";
    print STDERR "  OR git users can set 'git config user.name'\n";
    exit(1);
}

sub changeLogName()
{
    my $name = $ENV{CHANGE_LOG_NAME} || $ENV{REAL_NAME} || gitConfig("user.name") || (split /\s*,\s*/, (getpwuid $<)[6])[0];

    changeLogNameError("Failed to determine ChangeLog name.") unless $name;
    # getpwuid seems to always succeed on windows, returning the username instead of the full name.  This check will catch that case.
    changeLogNameError("'$name' does not contain a space!  ChangeLogs should contain your full name.") unless ($name =~ /\w \w/);

    return $name;
}

sub changeLogEmailAddressError($)
{
    my ($message) = @_;
    print STDERR "$message\nEither:\n";
    print STDERR "  set CHANGE_LOG_EMAIL_ADDRESS in your environment\n";
    print STDERR "  OR pass --email= on the command line\n";
    print STDERR "  OR set EMAIL_ADDRESS in your environment\n";
    print STDERR "  OR git users can set 'git config user.email'\n";
    exit(1);
}

sub changeLogEmailAddress()
{
    my $emailAddress = $ENV{CHANGE_LOG_EMAIL_ADDRESS} || $ENV{EMAIL_ADDRESS} || gitConfig("user.email");

    changeLogEmailAddressError("Failed to determine email address for ChangeLog.") unless $emailAddress;
    changeLogEmailAddressError("Email address '$emailAddress' does not contain '\@' and is likely invalid.") unless ($emailAddress =~ /\@/);

    return $emailAddress;
}

# http://tools.ietf.org/html/rfc1924
sub decodeBase85($)
{
    my ($encoded) = @_;
    my %table;
    my @characters = ('0'..'9', 'A'..'Z', 'a'..'z', '!', '#', '$', '%', '&', '(', ')', '*', '+', '-', ';', '<', '=', '>', '?', '@', '^', '_', '`', '{', '|', '}', '~');
    for (my $i = 0; $i < 85; $i++) {
        $table{$characters[$i]} = $i;
    }

    my $decoded = '';
    my @encodedChars = $encoded =~ /./g;

    for (my $encodedIter = 0; defined($encodedChars[$encodedIter]);) {
        my $digit = 0;
        for (my $i = 0; $i < 5; $i++) {
            $digit *= 85;
            my $char = $encodedChars[$encodedIter];
            $digit += $table{$char};
            $encodedIter++;
        }

        for (my $i = 0; $i < 4; $i++) {
            $decoded .= chr(($digit >> (3 - $i) * 8) & 255);
        }
    }

    return $decoded;
}

sub decodeGitBinaryChunk($$)
{
    my ($contents, $fullPath) = @_;

    # Load this module lazily in case the user don't have this module
    # and won't handle git binary patches.
    require Compress::Zlib;

    my $encoded = "";
    my $compressedSize = 0;
    while ($contents =~ /^([A-Za-z])(.*)$/gm) {
        my $line = $2;
        next if $line eq "";
        die "$fullPath: unexpected size of a line: $&" if length($2) % 5 != 0;
        my $actualSize = length($2) / 5 * 4;
        my $encodedExpectedSize = ord($1);
        my $expectedSize = $encodedExpectedSize <= ord("Z") ? $encodedExpectedSize - ord("A") + 1 : $encodedExpectedSize - ord("a") + 27;

        die "$fullPath: unexpected size of a line: $&" if int(($expectedSize + 3) / 4) * 4 != $actualSize;
        $compressedSize += $expectedSize;
        $encoded .= $line;
    }

    my $compressed = decodeBase85($encoded);
    $compressed = substr($compressed, 0, $compressedSize);
    return Compress::Zlib::uncompress($compressed);
}

sub decodeGitBinaryPatch($$)
{
    my ($contents, $fullPath) = @_;

    # Git binary patch has two chunks. One is for the normal patching
    # and another is for the reverse patching.
    #
    # Each chunk a line which starts from either "literal" or "delta",
    # followed by a number which specifies decoded size of the chunk.
    # The "delta" type chunks aren't supported by this function yet.
    #
    # Then, content of the chunk comes. To decode the content, we
    # need decode it with base85 first, and then zlib.
    my $gitPatchRegExp = '(literal|delta) ([0-9]+)\n([A-Za-z0-9!#$%&()*+-;<=>?@^_`{|}~\\n]*?)\n\n';
    if ($contents !~ m"\nGIT binary patch\n$gitPatchRegExp$gitPatchRegExp\Z") {
        die "$fullPath: unknown git binary patch format"
    }

    my $binaryChunkType = $1;
    my $binaryChunkExpectedSize = $2;
    my $encodedChunk = $3;
    my $reverseBinaryChunkType = $4;
    my $reverseBinaryChunkExpectedSize = $5;
    my $encodedReverseChunk = $6;

    my $binaryChunk = decodeGitBinaryChunk($encodedChunk, $fullPath);
    my $binaryChunkActualSize = length($binaryChunk);
    my $reverseBinaryChunk = decodeGitBinaryChunk($encodedReverseChunk, $fullPath);
    my $reverseBinaryChunkActualSize = length($reverseBinaryChunk);

    die "$fullPath: unexpected size of the first chunk (expected $binaryChunkExpectedSize but was $binaryChunkActualSize" if ($binaryChunkExpectedSize != $binaryChunkActualSize);
    die "$fullPath: unexpected size of the second chunk (expected $reverseBinaryChunkExpectedSize but was $reverseBinaryChunkActualSize" if ($reverseBinaryChunkExpectedSize != $reverseBinaryChunkActualSize);

    return ($binaryChunkType, $binaryChunk, $reverseBinaryChunkType, $reverseBinaryChunk);
}

1;
