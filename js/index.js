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

function loadWasm() {
    var canvas = document.getElementById("my-canvas");
    var status = document.getElementById("my-status");
    qtLoader = new QtLoader({
        moduleConfig: {
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
        },
        canvasElements: [canvas],
        showLoader: function(loaderStatus) {
          canvas.style.display = 'none';
          status.style.display = 'block';
          status.innerHTML = loaderStatus + "...";
        },
        showError: function(errorText) {
          canvas.style.display = 'none';
          status.style.display = 'block';
          status.innerHTML = errorText;
        },
        showExit: function() {
          status.innerHTML = "Application exit";
          if (qtLoader.exitCode !== undefined)
              status.innerHTML += " with code " + qtLoader.exitCode;
          if (qtLoader.exitText !== undefined)
              status.innerHTML += " (" + qtLoader.exitText + ")";
          canvas.style.display = 'none';
          status.style.display = 'block';
        },
        showCanvas: function() {
          canvas.style.display = 'block';
          status.style.display = 'none';
        },
        // statusChanged: function(newStatus) {
        //     switch (newStatus) {
        //         case "Running":
        //             main(); // this is an async function so idk itll queue it up in the event queue
        //             break;
        //     }
        // }
    });

    qtLoader.loadEmscriptenModule("JsSpimbot");
    return qtLoader;
}

var Loader;

window.onload = () => {
    Loader = loadWasm();
}

// var Module = {
//     print: (text) => {
//         Elements.output.insertAdjacentHTML("beforeend", text + "\n");
//         Elements.output.scrollTop = Elements.output.scrollHeight;
//         console.log("from  module: " + text);
//     },
//     printErr: (text) => {
//         Elements.log.insertAdjacentText("beforeend", text + "\n");
//         Elements.log.scrollTop = Elements.output.scrollHeight;
//         console.error("from  module: " + text);
//     },
//     onRuntimeInitialized: async () => {
//         console.log("Now setting up UI");
//         await main();
//     }
// };

async function main(fileInput = `Tests/${fileList[0]}`, ctx = null) {
    console.log("Running main load");
    if (ctx == null) {
        for (var ctx = 0; ctx < 2; ctx++) {
            let data = await loadData(fileInput);
            const stream = Loader.module().FS.open('input_'+ctx+'.s', 'w+');
            Loader.module().FS.write(stream, new Uint8Array(data), 0, data.byteLength, 0);
            Loader.module().FS.close(stream);
        }
        Execution.init();
    } else {
        let data = await loadData(fileInput);
        const stream = Loader.module().FS.open('input_'+ctx+'.s', 'w+');
        Loader.module().FS.write(stream, new Uint8Array(data), 0, data.byteLength, 0);
        Loader.module().FS.close(stream);
        Execution.init(false, ctx);
        changeContext(ctx);
        Elements.contextSelector.selectedIndex = ctx;
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

async function changeContext(ctx) {
    console.log("change ctx from ", Execution.ctx, " to ", ctx);
    Execution.ctx = ctx;
    console.log("current ctx: ", Execution.ctx);

    // Loader.module().pause();
    // Execution.playing = false;
    // Elements.playButton.innerHTML = "Continue";
    if (Loader.module().lockSimulator(100)) {
        updateStdOut(Execution.ctx);
        updateStdErr(Execution.ctx);
        // RegisterUtils.update(Execution.ctx);
        // MemoryUtils.update(Execution.ctx);
        RegisterUtils.init(Execution.ctx);
        MemoryUtils.init(Execution.ctx);
        InstructionUtils.update(Execution.ctx);

        Loader.module().unlockSimulator();
    }

}
