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
    } else {
        let data = await loadData(fileInput);
        const stream = FS.open('input_'+ctx+'.s', 'w+');
        FS.write(stream, new Uint8Array(data), 0, data.byteLength, 0);
        FS.close(stream);
    }

    Execution.init();
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
