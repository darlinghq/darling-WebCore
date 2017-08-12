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

const MarginForThreeButtonsOrLess = 24;
const MarginForFourButtons = 16;
const MarginForFiveButtons = 12;
const FullscreenTimeControlWidth = 457;

class MacOSFullscreenMediaControls extends MacOSMediaControls
{

    constructor(options = {})
    {
        options.layoutTraits = LayoutTraits.macOS | LayoutTraits.Fullscreen;

        super(options);

        this.element.classList.add("fullscreen");

        // Set up fullscreen-specific buttons.
        this.rewindButton = new RewindButton(this);
        this.forwardButton = new ForwardButton(this);
        this.fullscreenButton.isFullscreen = true;

        this.volumeSlider.width = 60;

        this._centerContainer = new ButtonsContainer({
            buttons: [this.rewindButton, this.playPauseButton, this.forwardButton],
            cssClassName: "center",
            padding: 27,
            margin: 27
        });

        this._rightContainer = new ButtonsContainer({
            buttons: [this.airplayButton, this.pipButton, this.tracksButton, this.fullscreenButton],
            cssClassName: "right",
            padding: 12
        });

        this.controlsBar.children = [this.volumeSlider, this._centerContainer, this._rightContainer, this.timeControl];

        this.element.addEventListener("mousedown", this);
    }

    // Public

    showTracksPanel()
    {
        super.showTracksPanel();

        const tracksButtonBounds = this.tracksButton.element.getBoundingClientRect();
        this.tracksPanel.rightX = window.innerWidth - tracksButtonBounds.right;
        this.tracksPanel.bottomY = window.innerHeight - tracksButtonBounds.top + 1;
    }

    // Protected

    handleEvent(event)
    {
        switch (event.type) {
        case "mousedown":
            this._handleMousedown(event);
            break;
        case "mousemove":
            this._handleMousemove(event);
            break;
        case "mouseup":
            this._handleMouseup(event);
            break;
        }
    }

    layout()
    {
        super.layout();

        const numberOfEnabledButtons = this._rightContainer.buttons.filter(button => button.enabled).length;

        let margin = MarginForFiveButtons;
        if (numberOfEnabledButtons === 4)
            margin = MarginForFourButtons;
        else if (numberOfEnabledButtons <= 3)
            margin = MarginForThreeButtonsOrLess;

        this._rightContainer.margin = margin;

        this._centerContainer.layout();
        this._rightContainer.layout();

        this.timeControl.width = FullscreenTimeControlWidth;
    }

    // Private

    _handleMousedown(event)
    {
        if (event.target !== this.controlsBar.element)
            return;

        event.preventDefault();

        this._lastDragPoint = this._pointForEvent(event);

        this.element.addEventListener("mousemove", this, true);
        this.element.addEventListener("mouseup", this, true);
    }

    _handleMousemove(event)
    {
        event.preventDefault();

        const currentDragPoint = this._pointForEvent(event);

        this.controlsBar.translation = new DOMPoint(
            this.controlsBar.translation.x + currentDragPoint.x - this._lastDragPoint.x,
            this.controlsBar.translation.y + currentDragPoint.y - this._lastDragPoint.y
        );
        
        this._lastDragPoint = currentDragPoint;
    }

    _handleMouseup(event)
    {
        event.preventDefault();

        delete this._lastDragPoint;

        this.element.removeEventListener("mousemove", this, true);
        this.element.removeEventListener("mouseup", this, true);
    }

    _pointForEvent(event)
    {
        return new DOMPoint(event.clientX, event.clientY);
    }

}
