class Execution {
    static init(reset = false) {
        Execution.maxSpeed = Elements.speedSelector.max;
        Execution.speed = Elements.speedSelector.value;

        Execution.started = false;
        Execution.playing = false;
        Execution.previousDrawTimes = [];
        Execution.skipBreakpoint = false;
        // Execution.cycles = 0;

        Elements.stepButton.disabled = false;
        Elements.playButton.disabled = false;
        Elements.playButton.innerHTML = (Execution.speed === Execution.maxSpeed) ? "Run" : "Play";

        Elements.output.innerHTML = '';
        Elements.log.innerHTML = '';

        Module.init();
        RegisterUtils.init();
        MemoryUtils.init();

        if (reset) {
            InstructionUtils.removeAllBreakpoints();
            InstructionUtils.highlightCurrentInstruction();
        } else {
            InstructionUtils.init();
            InstructionUtils.highlightCurrentInstruction();
        }
        if (Execution.updateDrawTmeId !== undefined)
            window.cancelAnimationFrame(Execution.updateDrawTmeId)
        Execution.updateDrawTmeId = window.requestAnimationFrame(Execution.updateDrawTme);
    }

    static step(stepSize = 1) {
        const result = Module.step(stepSize, Execution.playing ? Execution.skipBreakpoint : true);

        if (result === 0)  // finished
            Execution.finish();
        else if (result === -1) {  // break point encountered
            Execution.skipBreakpoint = true;
            Execution.playing = false;
            Elements.playButton.innerHTML = "Continue";
        } else if (result === 1) { // break point not encountered
            Execution.skipBreakpoint = false;
        }

        RegisterUtils.update();
        MemoryUtils.update();
        InstructionUtils.highlightCurrentInstruction();
    }

    static togglePlay() {
        Execution.started = true;
        if (Execution.playing) {
            Execution.playing = false;
            Elements.playButton.innerHTML = "Continue"
        } else {
            Execution.playing = true;
            Elements.playButton.innerHTML = "Pause";
            if (Execution.updateDrawTmeId !== undefined)
                window.cancelAnimationFrame(Execution.updateDrawTmeId)
            window.requestAnimationFrame(Execution.play);
        }
    }

    static updateDrawTme(timestamp) {
        Execution.previousDrawTimes.unshift(timestamp);
        if (Execution.previousDrawTimes.length > 6) {
            Execution.previousDrawTimes.pop();
        }

        if (Execution.playing) return;
        Execution.updateDrawTmeId = window.requestAnimationFrame(Execution.updateDrawTme);
    }

    static play(timestamp) {
        // We will run a set of steps right before we draw. That way, the event loop does not decide to speed through the
        //  simulation and we will not skip any frames. The speed consistency across machines now should be very similar.
        // Since reuestAnimationFrame is called on every time the display refrehses, in order to not speed up the simulation,
        //  we must scale the refresh rate so that the program steps 8192 cycles per 1/60 secs.
        // The slider now dictates how often we draw (i.e. draw after X number of cycles) in an exponential regression.
        if (!Execution.playing) {
            Execution.updateDrawTmeId = window.requestAnimationFrame(Execution.updateDrawTme);
            return;
        }
        if (Execution.previousDrawTimes.length == 0) {
            Execution.previousDrawTimes.push(Date.now());
        }

        Execution.updateDrawTme(timestamp);

        var refreshRateScale = 1 / 60 * 1000 / (Execution.getMedianRefreshRate());
        Execution.draw_cycle = Math.floor((322.502 * Math.exp(Execution.speed / 30.538) - 332.237) / refreshRateScale); // [1, 8192 / refreshRateScale] range given a domain of [1, 100]

        Execution.step(Execution.draw_cycle); // This number refers to the number of cycles to elapse before the program draws to the screen
        window.requestAnimationFrame(Execution.play);
    }

    static getMedianRefreshRate() {
        var refreshRatesMs = [];
        for (var i = 1; i < Execution.previousDrawTimes.length; i++) {
            refreshRatesMs.push(Execution.previousDrawTimes[i - 1] - Execution.previousDrawTimes[i]);
        }

        return median(refreshRatesMs);
    }

    static finish() {
        Execution.playing = false;

        Elements.playButton.disabled = true;
        Elements.stepButton.disabled = true;

        Elements.playButton.innerHTML = (Execution.speed === Execution.maxSpeed) ? "Run" : "Play";
    }

    static setSpeed(newSpeed) {
        Execution.speed = newSpeed;
        if (Execution.started) return;
        Elements.playButton.innerHTML = (Execution.speed === Execution.maxSpeed) ? "Run" : "Play";
    }
}

const median = arr => {
    const mid = Math.floor(arr.length / 2),
        nums = [...arr].sort((a, b) => a - b);
    return arr.length % 2 !== 0 ? nums[mid] : (nums[mid - 1] + nums[mid]) / 2;
};

Elements.resetButton.onclick = () => Execution.init(true);
Elements.stepButton.onclick = () => Execution.step(1);
Elements.playButton.onclick = () => Execution.togglePlay();