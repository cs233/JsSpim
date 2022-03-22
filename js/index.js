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
    postRun: [main],
    print: (text) => {
        Elements.output.innerHTML += text + "\n";
        Elements.output.scrollTop = Elements.output.scrollHeight;
    },
    printErr: (text) => {
        Elements.log.innerHTML += text + "\n";
        Elements.log.scrollTop = Elements.output.scrollHeight;
    }
};

async function main(fileInput = `Tests/${fileList[0]}`, context = 0) {
    let data = await loadData(fileInput);

    const stream = FS.open('input_'+context+'.s', 'w+');
    FS.write(stream, new Uint8Array(data), 0, data.byteLength, 0);
    FS.close(stream);

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