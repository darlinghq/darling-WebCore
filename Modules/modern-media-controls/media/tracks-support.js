/*
 * Copyright (C) 2016 Apple Inc. All Rights Reserved.
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

class TracksSupport extends MediaControllerSupport
{

    constructor(mediaController)
    {
        super(mediaController);

        if (!this.control)
            return;

        this.mediaController.controls.tracksPanel.dataSource = this;
        this.mediaController.controls.tracksPanel.uiDelegate = this;

        const media = mediaController.media;
        for (let tracks of [media.audioTracks, media.textTracks]) {
            for (let eventType of ["addtrack", "change", "removetrack"])
                tracks.addEventListener(eventType, this);
        }
    }

    // Public

    destroy()
    {
        super.destroy();

        const media = this.mediaController.media;
        for (let tracks of [media.audioTracks, media.textTracks]) {
            for (let eventType of ["addtrack", "change", "removetrack"])
                tracks.removeEventListener(eventType, this);
        }
    }

    // Protected

    get control()
    {
        return this.mediaController.controls.tracksButton;
    }

    get mediaEvents()
    {
        return ["loadedmetadata"];
    }

    buttonWasPressed(control)
    {
        this.mediaController.controls.showTracksPanel();
    }

    tracksPanelNumberOfSections()
    {
        let numberOfSections = 0;
        if (this._canPickAudioTracks())
            numberOfSections++;
        if (this._canPickTextTracks())
            numberOfSections++;
        return numberOfSections;
    }

    tracksPanelTitleForSection(sectionIndex)
    {
        if (sectionIndex == 0 && this._canPickAudioTracks())
            return UIString("Audio");
        return UIString("Subtitles");
    }

    tracksPanelNumberOfTracksInSection(sectionIndex)
    {
        if (sectionIndex == 0 && this._canPickAudioTracks())
            return this._audioTracks().length;
        return this._textTracks().length;
    }

    tracksPanelTitleForTrackInSection(trackIndex, sectionIndex)
    {
        let track;
        if (sectionIndex == 0 && this._canPickAudioTracks())
            track = this._audioTracks()[trackIndex];
        else
            track = this._textTracks()[trackIndex];

        if (this.mediaController.host)
            return this.mediaController.host.displayNameForTrack(track);
        return track.label;
    }

    tracksPanelIsTrackInSectionSelected(trackIndex, sectionIndex)
    {
        if (sectionIndex == 0 && this._canPickAudioTracks())
            return this._audioTracks()[trackIndex].enabled;
        return this._textTracks()[trackIndex].mode !== "disabled";
    }

    tracksPanelSelectionDidChange(trackIndex, sectionIndex)
    {
        if (sectionIndex == 0 && this._canPickAudioTracks()) {
            let track = this._audioTracks()[trackIndex];
            track.enabled = !track.enabled;
        } else {
            let track = this._textTracks()[trackIndex];
            track.mode = track.mode === "disabled" ? "showing" : "disabled";
        }

        this.mediaController.controls.hideTracksPanel();
    }

    syncControl()
    {
        this.control.enabled = this._canPickAudioTracks() || this._canPickTextTracks();
    }

    // Private

    _textTracks()
    {
        return this._sortedTrackList(this.mediaController.media.textTracks);
    }

    _audioTracks()
    {
        return this._sortedTrackList(this.mediaController.media.audioTracks);
    }

    _canPickAudioTracks()
    {
        const audioTracks = this._audioTracks();
        return audioTracks && audioTracks.length > 1;
    }

    _canPickTextTracks()
    {
        const textTracks = this._textTracks();
        return textTracks && textTracks.length > 0;
    }

    _sortedTrackList(tracks)
    {
        if (this.mediaController.host)
            return this.mediaController.host.sortedTrackListForMenu(tracks);
        return tracks;
    }

}
