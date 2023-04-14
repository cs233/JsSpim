const fileList = [
    'fibonacci.s',
    'test_core.s',
    'test_le.s',
    'test_sys_call.s',
    'hello_world.s',
    'read_string.s',
    'timing.s',
    'timer_interrupt.s'
];
fileList.forEach(filename => {
    const option0 = document.createElement("option");
    option0.text = filename;
    option0.value = `Tests/${filename}`;
    Elements.fileSelector0.add(option0);
    const option1 = document.createElement("option");
    option1.text = filename;
    option1.value = `Tests/${filename}`;
    Elements.fileSelector1.add(option1);
});

const programList = [
    'Program 1',
    'Program 2'
];
let ctx_var = 0;
programList.forEach(programName => {
    const option = document.createElement("option");
    option.text = programName;
    option.value = ctx_var;
    Elements.contextSelector.add(option);
    ctx_var++;
});

var Module = {
    print: (text) => {
        Elements.output.insertAdjacentHTML("beforeend", text + "\n");
        Elements.output.scrollTop = Elements.output.scrollHeight;
        console.log("from  module: " + text);
    },
    printErr: (text) => {
        Elements.log.insertAdjacentText("beforeend", text + "\n");
        Elements.log.scrollTop = Elements.output.scrollHeight;
        console.error("from  module: " + text);
    },
    onRuntimeInitialized: async () => {
        console.log("Now setting up UI");
        await main();
    }
};

async function main(fileInput = `Tests/${fileList[0]}`, ctx = null) {
    console.log("Running main load");
    if (ctx == null) {
        for (var ctx = 0; ctx < 2; ctx++) {
            let data = await loadData(fileInput);
            const stream = FS.open('input_'+ctx+'.s', 'w+');
            FS.write(stream, new Uint8Array(data), 0, data.byteLength, 0);
            FS.close(stream);
        }
        Execution.init();
    } else {
        let data = await loadData(fileInput);
        const stream = FS.open('input_'+ctx+'.s', 'w+');
        FS.write(stream, new Uint8Array(data), 0, data.byteLength, 0);
        FS.close(stream);

        // when upload file != enabled ctx, not change anything
        let cur_ctx = document.getElementById("context-selector").value;
        if (ctx == cur_ctx) {
            Execution.init(false, ctx);
            changeContext(ctx);


            const ctx_list = []; // [0]
            if (document.getElementById("context-enabler0").checked) {ctx_list.push(0)};
            if (document.getElementById("context-enabler1").checked) {ctx_list.push(1)};
            let new_ctx_idx = 0;
            for (var i = 0; i < ctx_list.length; i++) {
                if (ctx == ctx_list[i]) {new_ctx_idx = i; break;}
            }
            Elements.contextSelector.selectedIndex = new_ctx_idx;

            // Elements.contextSelector.selectedIndex = ctx;
            switch (ctx) {
                case 0:
                    Elements.fileSelector0.value = fileInput;
                    break;
                case 1:
                    Elements.fileSelector1.value = fileInput;
                    break;
                default:
            }
        }

    }

    // Execution.init();
}

async function loadData(fileInput) {
    if (fileInput instanceof File) { // local file
        const reader = new FileReader();
        return new Promise((resolve) => {
            reader.onload = () => resolve(reader.result);
            reader.readAsArrayBuffer(fileInput);
        });
    } else { // remote file
        const response = await fetch(fileInput);
        return response.arrayBuffer();
    }
}

let prev_ctx_list = [0, 1];

async function initModule() {

    let enable_ctx0 = document.getElementById("context-enabler0").checked;
    let enable_ctx1 = document.getElementById("context-enabler1").checked;
    console.log("program 0 enabled: ", enable_ctx0);
    console.log("program 1 enabled: ", enable_ctx1);

    // TODO: add module reset when
    const ctx_list = []; // [0]
    if (enable_ctx0) {ctx_list.push(0)};
    if (enable_ctx1) {ctx_list.push(1)};
    Module.reset(2, ctx_list);
    
    let cur_ctx = document.getElementById("context-selector").value;

    // 1. Re-initialize the context selector options (remove disabled ctx)
    Elements.contextSelector.innerHTML = '';
    ctx_list.forEach(ctx_idx => {
        const option = document.createElement("option");
        if (ctx_idx == 0) programName = "Program1";
        else if (ctx_idx == 1) programName = "Program2";

        option.text = programName;
        option.value = ctx_idx;
        Elements.contextSelector.add(option);
    });

    // 2. switch the option of context selector to enabled one
    // and switch the context to the enabled one

    // 1) disable a ctx
    if (prev_ctx_list.length > ctx_list.length) {
        let diff_ctx = prev_ctx_list.filter(item => !ctx_list.includes(item))[0];
        // From 1 => 0
        if (ctx_list.length == 0) {
            Elements.contextSelector.selectedIndex = -1;
            // clean frontend
            // Output 
            Elements.output.innerHTML = "";
            Elements.log.innerHTML = "";

            // Reg
            Elements.generalReg.innerHTML = "";
            Elements.specialReg.innerHTML = "";
            Elements.floatReg.innerHTML = "";
            Elements.doubleReg.innerHTML = "";

            // Insn
            Elements.userTextContent.innerHTML = "";
            Elements.kernelTextContent.innerHTML = "";
            Elements.kernelTextContainer.innerHTML = "";

            // Mem
            Elements.userData.innerHTML = "";
            Elements.kernelData.innerHTML = "";
            Elements.kernelDataContainer.innerHTML = "";
            Elements.stack.innerHTML = "";              
        }
        // From 2 => 1
        else {
            // diabled = selected => switch ctx to another
            if (diff_ctx == cur_ctx) {
                let new_ctx = ctx_list[0];
                // Elements.contextSelector.selectedIndex = new_ctx;
                Elements.contextSelector.selectedIndex = 0;

                Execution.ctx = new_ctx; 
                if (Module.lockSimulator(100)) {
                    updateStdOut(new_ctx);
                    updateStdErr(new_ctx);
                    RegisterUtils.init(new_ctx);
                    MemoryUtils.init(new_ctx);
                    InstructionUtils.update(new_ctx);
                    Module.unlockSimulator();
                }
            }
            // disable != selected
            else {
                // do nothing
            }
        }

    }
    // enable 
    else if (prev_ctx_list.length < ctx_list.length) {
        // From 1 => 2
        let diff_ctx = ctx_list.filter(item => !prev_ctx_list.includes(item))[0];
        let new_ctx = diff_ctx;
        Execution.ctx = new_ctx; 
        console.log("enable ctx, change to new ctx: ", new_ctx);

        let new_ctx_idx = 0;
        for (var i = 0; i < ctx_list.length; i++) {
            if (new_ctx == ctx_list[i]) {new_ctx_idx = i; break;}
        }
        Elements.contextSelector.selectedIndex = new_ctx_idx;
        if (Module.lockSimulator(100)) {
            updateStdOut(new_ctx);
            updateStdErr(new_ctx);
            RegisterUtils.init(new_ctx);
            MemoryUtils.init(new_ctx);
            InstructionUtils.update(new_ctx);
            Module.unlockSimulator();
        }
    }

    prev_ctx_list = ctx_list.slice();


    // 3. 





    // if (prev_ctx_list.length == ctx_list.length) {
    //     // same ctx list, no modification

    // } else if (prev_ctx_list.length > ctx_list.length) {
    //     // disabled selected ctx, clean frontend
    //     let diff_ctx = prev_ctx_list.filter(item => !ctx_list.includes(item))[0];
    //     if (diff_ctx == cur_ctx) {
    //         // Output 
    //         Elements.output.innerHTML = "";
    //         Elements.log.innerHTML = "";

    //         // Reg
    //         Elements.generalReg.innerHTML = "";
    //         Elements.specialReg.innerHTML = "";
    //         Elements.floatReg.innerHTML = "";
    //         Elements.doubleReg.innerHTML = "";

    //         // Insn
    //         Elements.userTextContent.innerHTML = "";
    //         Elements.kernelTextContent.innerHTML = "";
    //         Elements.kernelTextContainer.innerHTML = "";

    //         // Mem
    //         Elements.userData.innerHTML = "";
    //         Elements.kernelData.innerHTML = "";
    //         Elements.kernelDataContainer.innerHTML = "";
    //         Elements.stack.innerHTML = "";            
    //     }
        
    // } else {
    //     // enabled selected ctx, update frontend
    //     let diff_ctx = ctx_list.filter(item => !prev_ctx_list.includes(item))[0];
    //     if (diff_ctx == cur_ctx) {
    //         if (Module.lockSimulator(100)) {
    //             updateStdOut(cur_ctx);
    //             updateStdErr(cur_ctx);
    //             RegisterUtils.init(cur_ctx);
    //             MemoryUtils.init(cur_ctx);
    //             InstructionUtils.update(cur_ctx);
        
    //             Module.unlockSimulator();
    //         }
    //     }

    // }
    // prev_ctx_list = ctx_list.slice();
}

async function changeContext(ctx) {

    var ctx_enabled = false;
    if (ctx == 0) ctx_enabled = document.getElementById("context-enabler0").checked;
    else if (ctx == 1) ctx_enabled = document.getElementById("context-enabler1").checked;

    // If the ctx is not enabled, clean the front end
    if (!ctx_enabled) {
        // Output 
        Elements.output.innerHTML = "";
        Elements.log.innerHTML = "";

        // Reg
        Elements.generalReg.innerHTML = "";
        Elements.specialReg.innerHTML = "";
        Elements.floatReg.innerHTML = "";
        Elements.doubleReg.innerHTML = "";

        // Insn
        Elements.userTextContent.innerHTML = "";
        Elements.kernelTextContent.innerHTML = "";
        Elements.kernelTextContainer.innerHTML = "";

        // Mem
        Elements.userData.innerHTML = "";
        Elements.kernelData.innerHTML = "";
        Elements.kernelDataContainer.innerHTML = "";
        Elements.stack.innerHTML = "";
    }

    console.log("change ctx from ", Execution.ctx, " to ", ctx);
    Execution.ctx = ctx;
    console.log("current ctx: ", Execution.ctx);

    // Module.pause();
    // Execution.playing = false;
    // Elements.playButton.innerHTML = "Continue";


    if (ctx_enabled && Module.lockSimulator(100)) {
        updateStdOut(Execution.ctx);
        updateStdErr(Execution.ctx);
        // RegisterUtils.update(Execution.ctx);
        // MemoryUtils.update(Execution.ctx);
        RegisterUtils.init(Execution.ctx);
        MemoryUtils.init(Execution.ctx);
        InstructionUtils.update(Execution.ctx);

        Module.unlockSimulator();
    }

}
