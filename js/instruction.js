const worker = new Worker('js/highlight.min.js');
worker.onmessage = (event) => event.data.forEach((e, i) => InstructionUtils.instructionList[i].instructionElement.innerHTML = e);

class InstructionUtils {
    // static init() {
    //     Elements.userTextContent.innerHTML = '';
    //     Elements.kernelTextContent.innerHTML = '';

    //     const ctx = 0;
    //     const userText = Module.getUserText(ctx).split("\n").slice(0, -1).map(e => new Instruction(e, ctx));
    //     userText.forEach(e => Elements.userTextContent.appendChild(e.element));

    //     const kernelText = Module.getKernelText(ctx).split("\n").slice(0, -1).map(e => new Instruction(e, ctx));
    //     kernelText.forEach(e => Elements.kernelTextContent.appendChild(e.element));

    //     InstructionUtils.instructionList = [...userText, ...kernelText];

    //     InstructionUtils.instructionDict = {};
    //     userText.forEach(e => InstructionUtils.instructionDict[e.address] = e);
    //     kernelText.forEach(e => InstructionUtils.instructionDict[e.address] = e);

    //     InstructionUtils.formatCode();
    // }

    static init(ctx = 0) {
        Elements.userTextContent.innerHTML = '';
        Elements.kernelTextContent.innerHTML = '';

        this.ctx = ctx;
        this.userText = Module.getUserText(this.ctx).split("\n").slice(0, -1).map(e => new Instruction(e, this.ctx));
        this.userText.forEach(e => Elements.userTextContent.appendChild(e.element));

        this.kernelText = Module.getKernelText(this.ctx).split("\n").slice(0, -1).map(e => new Instruction(e, this.ctx));
        this.kernelText.forEach(e => Elements.kernelTextContent.appendChild(e.element));

        InstructionUtils.instructionList = [...this.userText, ...this.kernelText];

        InstructionUtils.instructionDict = {};
        this.userText.forEach(e => InstructionUtils.instructionDict[e.address] = e);
        this.kernelText.forEach(e => InstructionUtils.instructionDict[e.address] = e);

        this.breakpointAddr = [[],[]];

        InstructionUtils.formatCode();
    }

    static update(ctx) {
        Elements.userTextContent.innerHTML = '';
        Elements.kernelTextContent.innerHTML = '';

        this.ctx = ctx;

        this.userText = Module.getUserText(this.ctx).split("\n").slice(0, -1).map(e => new Instruction(e, this.ctx));
        this.userText.forEach(e => Elements.userTextContent.appendChild(e.element));

        this.kernelText = Module.getKernelText(this.ctx).split("\n").slice(0, -1).map(e => new Instruction(e, this.ctx));
        this.kernelText.forEach(e => Elements.kernelTextContent.appendChild(e.element));

        InstructionUtils.instructionList = [...this.userText, ...this.kernelText];

        InstructionUtils.instructionDict = {};
        this.userText.forEach(e => InstructionUtils.instructionDict[e.address] = e);
        this.kernelText.forEach(e => InstructionUtils.instructionDict[e.address] = e);

        InstructionUtils.formatCode();
        InstructionUtils.highlightCurrentInstruction();

        this.breakpointAddr[this.ctx].forEach(addr => InstructionUtils.instructionDict[addr].restoreBreakpoint());
    }

    static removeAllBreakpoints() {
        InstructionUtils.instructionList
            .filter(e => e.isBreakpoint)
            .forEach(e => {
                e.isBreakpoint = false;
                e.element.style.fontWeight = null;
            });
        
        this.breakpointAddr = [[],[]];
    }

    static highlightCurrentInstruction() {
        if (InstructionUtils.highlighted)
            InstructionUtils.highlighted.style.backgroundColor = null;

        const pc = RegisterUtils.getPC();
        const instruction = InstructionUtils.instructionDict[pc];
        if (!instruction) return;

        InstructionUtils.highlighted = instruction.element;
        InstructionUtils.highlighted.style.backgroundColor = 'yellow';
        InstructionUtils.highlighted.scrollIntoView({block: "nearest"});
    }

    static formatCode() {
        worker.postMessage(this.instructionList.map(e => e.instructionElement.innerHTML));
    }

    static toggleBinary(showBinary) {
        InstructionUtils.instructionList.forEach(e => {
            e.showBinary = showBinary;
            e.binaryElement.innerText = e.getBinaryInnerText();
        });
    }

    static toggleSourceCode(showSourceCode) {
        InstructionUtils.instructionList.forEach(e => {
            e.showSourceCode = showSourceCode;
            e.sourceCodeElement.innerText = e.getSourceCodeInnerText();
        });
    }

    static toggleKernelText(shoeKernelText) {
        if (shoeKernelText)
            Elements.kernelTextContainer.style.display = null;
        else
            Elements.kernelTextContainer.style.display = 'none';
    }
}

class Instruction {
    constructor(text, ctx) {
        this.text = text;
        this.ctx = ctx;

        this.isBreakpoint = false;
        this.showBinary = false;
        this.showSourceCode = true;

        this.addressString = this.text.substring(3, 11);
        this.address = Number.parseInt(this.addressString, 16);

        this.initElement()
    }

    initElement() {
        this.element = document.createElement("div");

        this.indexOfComma = this.text.indexOf(';');

        // address
        this.element.innerHTML = `[<span class="hljs-attr">${this.addressString}</span>] `;

        // instruction value
        this.binaryElement = document.createElement("span");
        this.binaryElement.innerText = this.getBinaryInnerText();
        this.binaryElement.classList.add("hljs-number");
        this.element.appendChild(this.binaryElement);

        // instruction
        this.instructionElement = document.createElement("span");
        this.instructionElement.innerText = this.getInstructionInnerText();
        this.element.appendChild(this.instructionElement);

        // source code
        this.sourceCodeElement = document.createElement("span");
        this.sourceCodeElement.classList.add("hljs-comment");
        this.sourceCodeElement.innerText = this.getSourceCodeInnerText();
        this.element.appendChild(this.sourceCodeElement);

        // add event listener
        this.element.onclick = () => this.toggleBreakpoint();
        return this.element;
    }

    getBinaryInnerText() {
        return this.showBinary ? this.text.substring(15, 24) : "";
    }

    getSourceCodeInnerText() {
        return (this.showSourceCode && this.indexOfComma > 0) ? this.text.substring(this.indexOfComma) : "";
    }

    getInstructionInnerText() {
        return this.indexOfComma > 0 ? this.text.substring(25, this.indexOfComma) : this.text.substring(25);
    }

    toggleBreakpoint() {
        this.isBreakpoint = !this.isBreakpoint;
        if (this.isBreakpoint) {
            Module.addBreakpoint(this.address, this.ctx);
            this.element.style.fontWeight = "bold";
            InstructionUtils.breakpointAddr[InstructionUtils.ctx].push(this.address);
        } else {
            Module.deleteBreakpoint(this.address, this.ctx);
            this.element.style.fontWeight = null;
            InstructionUtils.breakpointAddr[InstructionUtils.ctx].splice(InstructionUtils.breakpointAddr[InstructionUtils.ctx].indexOf(this.address), 1);
        }
    }

    restoreBreakpoint() {
        this.isBreakpoint = true;
        this.element.style.fontWeight = "bold";
    }
}
