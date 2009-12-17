# Copyright (C) 2009 Google Inc. All rights reserved.
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

import os

from optparse import make_option

from modules.comments import bug_comment_from_commit_text
from modules.grammar import pluralize
from modules.logging import log, error
from modules.webkitport import WebKitPort
from modules.changelogs import ChangeLog


class CommandOptions(object):
    force_clean = make_option("--force-clean", action="store_true", dest="force_clean", default=False, help="Clean working directory before applying patches (removes local changes and commits)")
    clean = make_option("--no-clean", action="store_false", dest="clean", default=True, help="Don't check if the working directory is clean before applying patches")
    check_builders = make_option("--ignore-builders", action="store_false", dest="check_builders", default=True, help="Don't check to see if the build.webkit.org builders are green before landing.")
    quiet = make_option("--quiet", action="store_true", dest="quiet", default=False, help="Produce less console output.")
    non_interactive = make_option("--non-interactive", action="store_true", dest="non_interactive", default=False, help="Never prompt the user, fail as fast as possible.")
    parent_command = make_option("--parent-command", action="store", dest="parent_command", default=None, help="(Internal) The command that spawned this instance.")
    update = make_option("--no-update", action="store_false", dest="update", default=True, help="Don't update the working directory.")
    local_commit = make_option("--local-commit", action="store_true", dest="local_commit", default=False, help="Make a local commit for each applied patch")
    build = make_option("--no-build", action="store_false", dest="build", default=True, help="Commit without building first, implies --no-test.")
    test = make_option("--no-test", action="store_false", dest="test", default=True, help="Commit without running run-webkit-tests.")
    close_bug = make_option("--no-close", action="store_false", dest="close_bug", default=True, help="Leave bug open after landing.")
    port = make_option("--port", action="store", dest="port", default=None, help="Specify a port (e.g., mac, qt, gtk, ...).")
    reviewer = make_option("-r", "--reviewer", action="store", type="string", dest="reviewer", help="Update ChangeLogs to say Reviewed by REVIEWER.")
    complete_rollout = make_option("--complete-rollout", action="store_true", dest="complete_rollout", help="Commit the revert and re-open the original bug.")


class AbstractStep(object):
    def __init__(self, tool, options):
        self._tool = tool
        self._options = options
        self._port = None

    def _run_script(self, script_name, quiet=False, port=WebKitPort):
        log("Running %s" % script_name)
        self._tool.executive.run_and_throw_if_fail(port.script_path(script_name), quiet)

    # FIXME: The port should live on the tool.
    def port(self):
        if self._port:
            return self._port
        self._port = WebKitPort.port(self._options.port)
        return self._port

    @classmethod
    def options(cls):
        return []

    def run(self, state):
        raise NotImplementedError, "subclasses must implement"


# FIXME: Unify with StepSequence?  I'm not sure yet which is the better design.
class MetaStep(AbstractStep):
    substeps = [] # Override in subclasses
    def __init__(self, tool, options):
        AbstractStep.__init__(self, tool, options)
        self._step_instances = []
        for step_class in self.substeps:
            self._step_instances.append(step_class(tool, options))

    @staticmethod
    def _collect_options_from_steps(steps):
        collected_options = []
        for step in steps:
            collected_options = collected_options + step.options()
        return collected_options

    @classmethod
    def options(cls):
        return cls._collect_options_from_steps(cls.substeps)

    def run(self, state):
        for step in self._step_instances:
             step.run(state)


class PrepareChangelogStep(AbstractStep):
    def run(self, state):
        self._run_script("prepare-ChangeLog")


class PrepareChangelogForRevertStep(AbstractStep):
    def run(self, state):
        # First, discard the ChangeLog changes from the rollout.
        os.chdir(self._tool.scm().checkout_root)
        changelog_paths = self._tool.scm().modified_changelogs()
        self._tool.scm().revert_files(changelog_paths)

        # Second, make new ChangeLog entries for this rollout.
        # This could move to prepare-ChangeLog by adding a --revert= option.
        self._run_script("prepare-ChangeLog")
        for changelog_path in changelog_paths:
            ChangeLog(changelog_path).update_for_revert(state["revision"])


class CleanWorkingDirectoryStep(AbstractStep):
    def __init__(self, tool, options, allow_local_commits=False):
        AbstractStep.__init__(self, tool, options)
        self._allow_local_commits = allow_local_commits

    @classmethod
    def options(cls):
        return [
            CommandOptions.force_clean,
            CommandOptions.clean,
        ]

    def run(self, state):
        os.chdir(self._tool.scm().checkout_root)
        if not self._allow_local_commits:
            self._tool.scm().ensure_no_local_commits(self._options.force_clean)
        if self._options.clean:
            self._tool.scm().ensure_clean_working_directory(force_clean=self._options.force_clean)


class CleanWorkingDirectoryWithLocalCommitsStep(CleanWorkingDirectoryStep):
    def __init__(self, tool, options):
        # FIXME: This a bit of a hack.  Consider doing this more cleanly.
        CleanWorkingDirectoryStep.__init__(self, tool, options, allow_local_commits=True)


class UpdateStep(AbstractStep):
    @classmethod
    def options(cls):
        return [
            CommandOptions.update,
            CommandOptions.port,
        ]

    def run(self, state):
        if not self._options.update:
            return
        log("Updating working directory")
        self._tool.executive.run_and_throw_if_fail(self.port().update_webkit_command(), quiet=True)


class ApplyPatchStep(AbstractStep):
    @classmethod
    def options(cls):
        return [
            CommandOptions.non_interactive,
        ]

    def run(self, state):
        log("Processing patch %s from bug %s." % (state["patch"]["id"], state["patch"]["bug_id"]))
        self._tool.scm().apply_patch(state["patch"], force=self._options.non_interactive)


class RevertRevisionStep(AbstractStep):
    def run(self, state):
        self._tool.scm().apply_reverse_diff(state["revision"])


class ApplyPatchWithLocalCommitStep(ApplyPatchStep):
    @classmethod
    def options(cls):
        return [
            CommandOptions.local_commit,
        ] + ApplyPatchStep.options()
    
    def run(self, state):
        ApplyPatchStep.run(self, state)
        if self._options.local_commit:
            commit_message = self._tool.scm().commit_message_for_this_commit()
            self._tool.scm().commit_locally_with_message(commit_message.message() or state["patch"]["name"])


class EnsureBuildersAreGreenStep(AbstractStep):
    @classmethod
    def options(cls):
        return [
            CommandOptions.check_builders,
        ]

    def run(self, state):
        if not self._options.check_builders:
            return
        red_builders_names = self._tool.buildbot.red_core_builders_names()
        if not red_builders_names:
            return
        red_builders_names = map(lambda name: "\"%s\"" % name, red_builders_names) # Add quotes around the names.
        error("Builders [%s] are red, please do not commit.\nSee http://%s.\nPass --ignore-builders to bypass this check." % (", ".join(red_builders_names), self._tool.buildbot.buildbot_host))


class EnsureLocalCommitIfNeeded(AbstractStep):
    @classmethod
    def options(cls):
        return [
            CommandOptions.local_commit,
        ]

    def run(self, state):
        if self._options.local_commit and not self._tool.scm().supports_local_commits():
            error("--local-commit passed, but %s does not support local commits" % self._tool.scm.display_name())


class UpdateChangelogsWithReviewerStep(AbstractStep):
    @classmethod
    def options(cls):
        return [
            CommandOptions.reviewer,
        ]

    def _guess_reviewer_from_bug(self, bug_id):
        patches = self._tool.bugs.fetch_reviewed_patches_from_bug(bug_id)
        if len(patches) != 1:
            log("%s on bug %s, cannot infer reviewer." % (pluralize("reviewed patch", len(patches)), bug_id))
            return None
        patch = patches[0]
        reviewer = patch["reviewer"]
        log("Guessing \"%s\" as reviewer from attachment %s on bug %s." % (reviewer, patch["id"], bug_id))
        return reviewer

    def run(self, state):
        bug_id = state["patch"]["bug_id"]
        reviewer = self._options.reviewer
        if not reviewer:
            if not bug_id:
                log("No bug id provided and --reviewer= not provided.  Not updating ChangeLogs with reviewer.")
                return
            reviewer = self._guess_reviewer_from_bug(bug_id)

        if not reviewer:
            log("Failed to guess reviewer from bug %s and --reviewer= not provided.  Not updating ChangeLogs with reviewer." % bug_id)
            return

        os.chdir(self._tool.scm().checkout_root)
        for changelog_path in self._tool.scm().modified_changelogs():
            ChangeLog(changelog_path).set_reviewer(reviewer)


class BuildStep(AbstractStep):
    @classmethod
    def options(cls):
        return [
            CommandOptions.build,
            CommandOptions.quiet,
        ]

    def run(self, state):
        if not self._options.build:
            return
        log("Building WebKit")
        self._tool.executive.run_and_throw_if_fail(self.port().build_webkit_command(), self._options.quiet)


class CheckStyleStep(AbstractStep):
    def run(self, state):
        self._run_script("check-webkit-style")


class RunTestsStep(AbstractStep):
    @classmethod
    def options(cls):
        return [
            CommandOptions.build,
            CommandOptions.test,
            CommandOptions.non_interactive,
            CommandOptions.quiet,
            CommandOptions.port,
        ]

    def run(self, state):
        if not self._options.build:
            return
        if not self._options.test:
            return
        args = self.port().run_webkit_tests_command()
        if self._options.non_interactive:
            args.append("--no-launch-safari")
            args.append("--exit-after-n-failures=1")
        if self._options.quiet:
            args.append("--quiet")
        self._tool.executive.run_and_throw_if_fail(args)


class CommitStep(AbstractStep):
    def run(self, state):
        commit_message = self._tool.scm().commit_message_for_this_commit()
        state["commit_text"] =  self._tool.scm().commit_with_message(commit_message.message())


class ClosePatchStep(AbstractStep):
    def run(self, state):
        comment_text = bug_comment_from_commit_text(self._tool.scm(), state["commit_text"])
        self._tool.bugs.clear_attachment_flags(state["patch"]["id"], comment_text)


class CloseBugStep(AbstractStep):
    @classmethod
    def options(cls):
        return [
            CommandOptions.close_bug,
        ]

    def run(self, state):
        if not self._options.close_bug:
            return
        # Check to make sure there are no r? or r+ patches on the bug before closing.
        # Assume that r- patches are just previous patches someone forgot to obsolete.
        patches = self._tool.bugs.fetch_patches_from_bug(state["patch"]["bug_id"])
        for patch in patches:
            review_flag = patch.get("review")
            if review_flag == "?" or review_flag == "+":
                log("Not closing bug %s as attachment %s has review=%s.  Assuming there are more patches to land from this bug." % (patch["bug_id"], patch["id"], review_flag))
                return
        self._tool.bugs.close_bug_as_fixed(state["patch"]["bug_id"], "All reviewed patches have been landed.  Closing bug.")


class CloseBugForLandDiffStep(AbstractStep):
    @classmethod
    def options(cls):
        return [
            CommandOptions.close_bug,
        ]

    def run(self, state):
        comment_text = bug_comment_from_commit_text(self._tool.scm(), state["commit_text"])
        bug_id = state["patch"]["bug_id"]
        if bug_id:
            log("Updating bug %s" % bug_id)
            if self._options.close_bug:
                self._tool.bugs.close_bug_as_fixed(bug_id, comment_text)
            else:
                # FIXME: We should a smart way to figure out if the patch is attached
                # to the bug, and if so obsolete it.
                self._tool.bugs.post_comment_to_bug(bug_id, comment_text)
        else:
            log(comment_text)
            log("No bug id provided.")


class CompleteRollout(MetaStep):
    substeps = [
        BuildStep,
        CommitStep,
    ]

    @classmethod
    def options(cls):
        collected_options = cls._collect_options_from_steps(cls.substeps)
        collected_options.append(CommandOptions.complete_rollout)
        return collected_options

    def run(self, state):
        bug_id = state["bug_id"]
        # FIXME: Fully automated rollout is not 100% idiot-proof yet, so for now just log with instructions on how to complete the rollout.
        # Once we trust rollout we will remove this option.
        if not self._options.complete_rollout:
            log("\nNOTE: Rollout support is experimental.\nPlease verify the rollout diff and use \"bugzilla-tool land-diff %s\" to commit the rollout." % bug_id)
            return

        MetaStep.run(self, state)

        if not bug_id:
            log(state["commit_text"])
            log("No bugs were updated or re-opened to reflect this rollout.")
            return
        # FIXME: I'm not sure state["commit_text"] is quite right here.
        self._tool.bugs.reopen_bug(bug_id, state["commit_text"])
