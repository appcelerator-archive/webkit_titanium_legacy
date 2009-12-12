/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

const UserInitiatedProfileName = "org.webkit.profiles.user-initiated";

WebInspector.ProfileType = function(id, name)
{
    this._id = id;
    this._name = name;
}

WebInspector.ProfileType.URLRegExp = /webkit-profile:\/\/(.+)\/(.+)#([0-9]+)/;

WebInspector.ProfileType.prototype = {
    get buttonTooltip()
    {
        return "";
    },

    get buttonStyle()
    {
        return undefined;
    },

    get buttonCaption()
    {
        return this.name;
    },

    get id()
    {
        return this._id;
    },

    get name()
    {
        return this._name;
    },

    buttonClicked: function()
    {
    },

    viewForProfile: function(profile)
    {
        if (!profile._profileView)
            profile._profileView = this.createView(profile);
        return profile._profileView;
    },

    // Must be implemented by subclasses.
    createView: function(profile)
    {
        throw new Error("Needs implemented.");
    },

    // Must be implemented by subclasses.
    createSidebarTreeElementForProfile: function(profile)
    {
        throw new Error("Needs implemented.");
    }
}

WebInspector.ProfilesPanel = function()
{
    WebInspector.Panel.call(this);

    this.createSidebar();

    this.element.addStyleClass("profiles");
    this._profileTypesByIdMap = {};
    this._profileTypeButtonsByIdMap = {};

    var panelEnablerHeading = WebInspector.UIString("You need to enable profiling before you can use the Profiles panel.");
    var panelEnablerDisclaimer = WebInspector.UIString("Enabling profiling will make scripts run slower.");
    var panelEnablerButton = WebInspector.UIString("Enable Profiling");
    this.panelEnablerView = new WebInspector.PanelEnablerView("profiles", panelEnablerHeading, panelEnablerDisclaimer, panelEnablerButton);
    this.panelEnablerView.addEventListener("enable clicked", this._enableProfiling, this);

    this.element.appendChild(this.panelEnablerView.element);

    this.profileViews = document.createElement("div");
    this.profileViews.id = "profile-views";
    this.element.appendChild(this.profileViews);

    this.enableToggleButton = new WebInspector.StatusBarButton("", "enable-toggle-status-bar-item");
    this.enableToggleButton.addEventListener("click", this._toggleProfiling.bind(this), false);

    this.profileViewStatusBarItemsContainer = document.createElement("div");
    this.profileViewStatusBarItemsContainer.id = "profile-view-status-bar-items";

    this._profiles = [];
    this.reset();
}

WebInspector.ProfilesPanel.prototype = {
    toolbarItemClass: "profiles",

    get toolbarItemLabel()
    {
        return WebInspector.UIString("Profiles");
    },

    get statusBarItems()
    {
        function clickHandler(profileType, buttonElement)
        {
            profileType.buttonClicked.call(profileType);
            this.updateProfileTypeButtons();
        }

        var items = [this.enableToggleButton.element];
        // FIXME: Generate a single "combo-button".
        for (var typeId in this._profileTypesByIdMap) {
            var profileType = this.getProfileType(typeId);
            if (profileType.buttonStyle) {
                var button = new WebInspector.StatusBarButton(profileType.buttonTooltip, profileType.buttonStyle, profileType.buttonCaption);
                this._profileTypeButtonsByIdMap[typeId] = button.element;
                button.element.addEventListener("click", clickHandler.bind(this, profileType, button.element), false);
                items.push(button.element);
            }
        }
        items.push(this.profileViewStatusBarItemsContainer);
        return items;
    },

    show: function()
    {
        WebInspector.Panel.prototype.show.call(this);
        if (this._shouldPopulateProfiles)
            this._populateProfiles();
    },

    populateInterface: function()
    {
        if (this.visible)
            this._populateProfiles();
        else
            this._shouldPopulateProfiles = true;
    },

    profilerWasEnabled: function()
    {
        this.reset();
        this.populateInterface();
    },

    profilerWasDisabled: function()
    {
        this.reset();
    },

    reset: function()
    {
        for (var i = 0; i < this._profiles.length; ++i)
            delete this._profiles[i]._profileView;

        delete this.currentQuery;
        this.searchCanceled();

        this._profiles = [];
        this._profilesIdMap = {};
        this._profileGroups = {};
        this._profileGroupsForLinks = {}

        this.sidebarTreeElement.removeStyleClass("some-expandable");

        for (var typeId in this._profileTypesByIdMap)
            this.getProfileType(typeId).treeElement.removeChildren();

        this.profileViews.removeChildren();

        this.profileViewStatusBarItemsContainer.removeChildren();

        this._updateInterface();
    },

    registerProfileType: function(profileType)
    {
        this._profileTypesByIdMap[profileType.id] = profileType;
        profileType.treeElement = new WebInspector.SidebarSectionTreeElement(profileType.name, null, true);
        this.sidebarTree.appendChild(profileType.treeElement);
        profileType.treeElement.expand();
    },

    _makeKey: function(text, profileTypeId)
    {
        return escape(text) + '/' + escape(profileTypeId);
    },

    addProfileHeader: function(profile)
    {
        var typeId = profile.typeId;
        var profileType = this.getProfileType(typeId);
        var sidebarParent = profileType.treeElement;
        var small = false;
        var alternateTitle;

        profile.__profilesPanelProfileType = profileType;
        this._profiles.push(profile);
        this._profilesIdMap[this._makeKey(profile.uid, typeId)] = profile;

        if (profile.title.indexOf(UserInitiatedProfileName) !== 0) {
            var profileTitleKey = this._makeKey(profile.title, typeId);
            if (!(profileTitleKey in this._profileGroups))
                this._profileGroups[profileTitleKey] = [];

            var group = this._profileGroups[profileTitleKey];
            group.push(profile);

            if (group.length === 2) {
                // Make a group TreeElement now that there are 2 profiles.
                group._profilesTreeElement = new WebInspector.ProfileGroupSidebarTreeElement(profile.title);

                // Insert at the same index for the first profile of the group.
                var index = sidebarParent.children.indexOf(group[0]._profilesTreeElement);
                sidebarParent.insertChild(group._profilesTreeElement, index);

                // Move the first profile to the group.
                var selected = group[0]._profilesTreeElement.selected;
                sidebarParent.removeChild(group[0]._profilesTreeElement);
                group._profilesTreeElement.appendChild(group[0]._profilesTreeElement);
                if (selected) {
                    group[0]._profilesTreeElement.select();
                    group[0]._profilesTreeElement.reveal();
                }

                group[0]._profilesTreeElement.small = true;
                group[0]._profilesTreeElement.mainTitle = WebInspector.UIString("Run %d", 1);

                this.sidebarTreeElement.addStyleClass("some-expandable");
            }

            if (group.length >= 2) {
                sidebarParent = group._profilesTreeElement;
                alternateTitle = WebInspector.UIString("Run %d", group.length);
                small = true;
            }
        }

        var profileTreeElement = profileType.createSidebarTreeElementForProfile(profile);
        profileTreeElement.small = small;
        if (alternateTitle)
            profileTreeElement.mainTitle = alternateTitle;
        profile._profilesTreeElement = profileTreeElement;

        sidebarParent.appendChild(profileTreeElement);
        if (!this.visibleView)
            this.showProfile(profile);
    },

    showProfile: function(profile)
    {
        if (!profile)
            return;

        if (this.visibleView)
            this.visibleView.hide();

        var view = profile.__profilesPanelProfileType.viewForProfile(profile);

        view.show(this.profileViews);

        profile._profilesTreeElement.select(true);
        profile._profilesTreeElement.reveal();

        this.visibleView = view;

        this.profileViewStatusBarItemsContainer.removeChildren();

        var statusBarItems = view.statusBarItems;
        for (var i = 0; i < statusBarItems.length; ++i)
            this.profileViewStatusBarItemsContainer.appendChild(statusBarItems[i]);
    },

    showView: function(view)
    {
        this.showProfile(view.profile);
    },

    getProfileType: function(typeId)
    {
        return this._profileTypesByIdMap[typeId];
    },

    showProfileForURL: function(url)
    {
        var match = url.match(WebInspector.ProfileType.URLRegExp);
        if (!match)
            return;
        this.showProfile(this._profilesIdMap[this._makeKey(match[3], match[1])]);
    },

    updateProfileTypeButtons: function()
    {
        for (var typeId in this._profileTypeButtonsByIdMap) {
            var buttonElement = this._profileTypeButtonsByIdMap[typeId];
            var profileType = this.getProfileType(typeId);
            buttonElement.className = profileType.buttonStyle;
            buttonElement.title = profileType.buttonTooltip;
            // FIXME: Apply profileType.buttonCaption once captions are added to button controls.
        }
    },

    closeVisibleView: function()
    {
        if (this.visibleView)
            this.visibleView.hide();
        delete this.visibleView;
    },

    displayTitleForProfileLink: function(title, typeId)
    {
        title = unescape(title);
        if (title.indexOf(UserInitiatedProfileName) === 0) {
            title = WebInspector.UIString("Profile %d", title.substring(UserInitiatedProfileName.length + 1));
        } else {
            var titleKey = this._makeKey(title, typeId);
            if (!(titleKey in this._profileGroupsForLinks))
                this._profileGroupsForLinks[titleKey] = 0;

            groupNumber = ++this._profileGroupsForLinks[titleKey];

            if (groupNumber > 2)
                // The title is used in the console message announcing that a profile has started so it gets
                // incremented twice as often as it's displayed
                title += " " + WebInspector.UIString("Run %d", groupNumber / 2);
        }
        
        return title;
    },

    get searchableViews()
    {
        var views = [];

        const visibleView = this.visibleView;
        if (visibleView && visibleView.performSearch)
            views.push(visibleView);

        var profilesLength = this._profiles.length;
        for (var i = 0; i < profilesLength; ++i) {
            var profile = this._profiles[i];
            var view = profile.__profilesPanelProfileType.viewForProfile(profile);
            if (!view.performSearch || view === visibleView)
                continue;
            views.push(view);
        }

        return views;
    },

    searchMatchFound: function(view, matches)
    {
        view.profile._profilesTreeElement.searchMatches = matches;
    },

    searchCanceled: function(startingNewSearch)
    {
        WebInspector.Panel.prototype.searchCanceled.call(this, startingNewSearch);

        if (!this._profiles)
            return;

        for (var i = 0; i < this._profiles.length; ++i) {
            var profile = this._profiles[i];
            profile._profilesTreeElement.searchMatches = 0;
        }
    },

    resize: function()
    {
        var visibleView = this.visibleView;
        if (visibleView && "resize" in visibleView)
            visibleView.resize();
    },

    _updateInterface: function()
    {
        // FIXME: Replace ProfileType-specific button visibility changes by a single ProfileType-agnostic "combo-button" visibility change.
        if (InspectorBackend.profilerEnabled()) {
            this.enableToggleButton.title = WebInspector.UIString("Profiling enabled. Click to disable.");
            this.enableToggleButton.toggled = true;
            for (var typeId in this._profileTypeButtonsByIdMap)
                this._profileTypeButtonsByIdMap[typeId].removeStyleClass("hidden");
            this.profileViewStatusBarItemsContainer.removeStyleClass("hidden");
            this.panelEnablerView.visible = false;
        } else {
            this.enableToggleButton.title = WebInspector.UIString("Profiling disabled. Click to enable.");
            this.enableToggleButton.toggled = false;
            for (var typeId in this._profileTypeButtonsByIdMap)
                this._profileTypeButtonsByIdMap[typeId].addStyleClass("hidden");
            this.profileViewStatusBarItemsContainer.addStyleClass("hidden");
            this.panelEnablerView.visible = true;
        }
    },

    _enableProfiling: function()
    {
        if (InspectorBackend.profilerEnabled())
            return;
        this._toggleProfiling(this.panelEnablerView.alwaysEnabled);
    },

    _toggleProfiling: function(optionalAlways)
    {
        if (InspectorBackend.profilerEnabled())
            InspectorBackend.disableProfiler(true);
        else
            InspectorBackend.enableProfiler(!!optionalAlways);
    },

    _populateProfiles: function()
    {
        var sidebarTreeChildrenCount = this.sidebarTree.children.length;
        for (var i = 0; i < sidebarTreeChildrenCount; ++i) {
            var treeElement = this.sidebarTree.children[i];
            if (treeElement.children.length)
                return;
        }

        function populateCallback(profileHeaders) {
            profileHeaders.sort(function(a, b) { return a.uid - b.uid; });
            var profileHeadersLength = profileHeaders.length;
            for (var i = 0; i < profileHeadersLength; ++i)
                WebInspector.addProfileHeader(profileHeaders[i]);
        }

        var callId = WebInspector.Callback.wrap(populateCallback);
        InspectorBackend.getProfileHeaders(callId);

        delete this._shouldPopulateProfiles;
    },

    updateMainViewWidth: function(width)
    {
        this.profileViews.style.left = width + "px";
        this.profileViewStatusBarItemsContainer.style.left = width + "px";
    }
}

WebInspector.ProfilesPanel.prototype.__proto__ = WebInspector.Panel.prototype;

WebInspector.ProfileSidebarTreeElement = function(profile)
{
    this.profile = profile;

    if (this.profile.title.indexOf(UserInitiatedProfileName) === 0)
        this._profileNumber = this.profile.title.substring(UserInitiatedProfileName.length + 1);

    WebInspector.SidebarTreeElement.call(this, "profile-sidebar-tree-item", "", "", profile, false);

    this.refreshTitles();
}

WebInspector.ProfileSidebarTreeElement.prototype = {
    onselect: function()
    {
        WebInspector.panels.profiles.showProfile(this.profile);
    },

    get mainTitle()
    {
        if (this._mainTitle)
            return this._mainTitle;
        if (this.profile.title.indexOf(UserInitiatedProfileName) === 0)
            return WebInspector.UIString("Profile %d", this._profileNumber);
        return this.profile.title;
    },

    set mainTitle(x)
    {
        this._mainTitle = x;
        this.refreshTitles();
    },

    get subtitle()
    {
        // There is no subtitle.
    },

    set subtitle(x)
    {
        // Can't change subtitle.
    },

    set searchMatches(matches)
    {
        if (!matches) {
            if (!this.bubbleElement)
                return;
            this.bubbleElement.removeStyleClass("search-matches");
            this.bubbleText = "";
            return;
        }

        this.bubbleText = matches;
        this.bubbleElement.addStyleClass("search-matches");
    }
}

WebInspector.ProfileSidebarTreeElement.prototype.__proto__ = WebInspector.SidebarTreeElement.prototype;

WebInspector.ProfileGroupSidebarTreeElement = function(title, subtitle)
{
    WebInspector.SidebarTreeElement.call(this, "profile-group-sidebar-tree-item", title, subtitle, null, true);
}

WebInspector.ProfileGroupSidebarTreeElement.prototype = {
    onselect: function()
    {
        WebInspector.panels.profiles.showProfile(this.children[this.children.length - 1].profile);
    }
}

WebInspector.ProfileGroupSidebarTreeElement.prototype.__proto__ = WebInspector.SidebarTreeElement.prototype;

WebInspector.didGetProfileHeaders = WebInspector.Callback.processCallback;
WebInspector.didGetProfile = WebInspector.Callback.processCallback;
