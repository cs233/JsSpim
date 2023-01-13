class Execution {
    static init(reset = false) {
        Execution.maxSpeed = Elements.speedSelector.max;
        Execution.speed = Elements.speedSelector.value;

        Execution.started = false;
        Execution.playing = false;
        Execution.previousDrawTimes = [];
        Execution.skipBreakpoint = false;
        Execution.maxCyclesAt60Hz = 8192;
        Execution.minCyclesAt60Hz = 1 / 60;
        Execution.cycleSkipCount = 0;
        // Execution.cycles = 0;

        Elements.stepButton.disabled = false;
        Elements.playButton.disabled = false;
        Elements.playButton.innerHTML = (Execution.speed === Execution.maxSpeed) ? "Run" : "Play";

        Elements.output.innerHTML = '';
        Elements.log.innerHTML = '';

        Module.reset(); // 2, [0, 1]);
        while (!Module.lockSimulator(100));
        RegisterUtils.init();
        MemoryUtils.init();

        if (reset) {
            InstructionUtils.removeAllBreakpoints();
            InstructionUtils.highlightCurrentInstruction();
        } else {
            InstructionUtils.init();
            InstructionUtils.highlightCurrentInstruction();
        }
        Module.unlockSimulator();

        if (Execution.updateDrawTmeId !== undefined)
            window.cancelAnimationFrame(Execution.updateDrawTmeId)
        Execution.updateDrawTmeId = window.requestAnimationFrame(Execution.updateDrawTme);
    }

    static step(stepSize = 1) {
        const result = Module.step(stepSize); // , Execution.playing ? Execution.skipBreakpoint : true);

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
        // if (!Execution.started) { // TODO: REMOVE. TEMPORARY FIX FOR PROOF OF CONCEPT
        //     Module.run_entire_program();
        // }
        Execution.started = true;
        if (Execution.playing) {
            Module.pause();
            Execution.playing = false;
            Elements.playButton.innerHTML = "Continue"
        } else {
            Module.play();
            Execution.playing = true;
            Elements.playButton.innerHTML = "Pause";
            if (Execution.updateDrawTmeId !== undefined)
                window.cancelAnimationFrame(Execution.updateDrawTmeId)
            window.requestAnimationFrame(Execution.updateUI);
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
        Execution.draw_cycle = Execution.getDrawCycleStep(Execution.speed, refreshRateScale);

        if (Execution.draw_cycle < 1) {
            Execution.cycleSkipCount++;
            if (Execution.cycleSkipCount * Execution.draw_cycle >= 1) {
                Execution.cycleSkipCount = 0;
                Execution.step();
            }
        } else {
            Execution.step(Math.floor(Execution.draw_cycle)); // This number refers to the number of cycles to elapse before the program draws to the screen
        }
        // window.requestAnimationFrame(Execution.play);
        window.requestAnimationFrame(Execution.updateUI);
    }

    static updateUI(_timestamp) {
        if (Execution.playing) {
            if (Module.lockSimulator(100)) { // make this magic number related to the refresh rate of the monitor
                RegisterUtils.update();
                MemoryUtils.update();
                InstructionUtils.highlightCurrentInstruction();

                Module.unlockSimulator();
            }
            window.requestAnimationFrame(Execution.updateUI);
        }
    }

    static getDrawCycleStep(speed, refreshRateScale) {
        // [1 / (30 * refreshRateScale), 8192 / refreshRateScale] range given a domain speed of [1, 100]
        if (speed >= 20) {
            // Exponential after speed of 20
            const a = 32.8123;
            var c = ((Execution.maxCyclesAt60Hz - 1) / refreshRateScale) / (Math.exp(100 / a) - Math.exp(20 / a));
            var b = 1 / refreshRateScale - c * Math.exp(20 / a);
            return c * Math.exp(speed / a) + b;
        } else {
            // Linear below 20
            return (29 * speed - 10) / (570 * refreshRateScale);
        }
    }

    static getCycleDelay(speed) {
        // [1 / 30, 8192] cycles range given a domain speed of [1, 100]
        // need to calculate seconds / cycle
        // [0.5 sec, 2.03 usec]
        if (speed >= 100) {
            // do not delay whatsoever
            return 0;
        } else if (speed >= 20) {
            // Exponential after speed of 20
            // range of [25000 usec, 2.03 usec)
            const b = 1.1249119103644276; // assume base 2: 2 ** ((log(25000) - log(10 ** 6 / (8192 * 60))) / 80)
            const a = 263214.8025904987; // 25000 / b ** -20
            return Math.round(a * Math.pow(b, -speed));
        } else {
            // Linear below 20
            return Math.round(-23750 * speed + 0.5e6);
        }
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
        Module.setDelay(Execution.getCycleDelay(newSpeed));
        console.log("Set speed delay to " + Execution.getCycleDelay(newSpeed) + "usec");
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
