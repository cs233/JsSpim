class MemoryUtils {
    static init(ctx = 0) {
        this.userData = new UserData(ctx);
        this.kernelData = new KernelData(ctx);
        this.stack = new Stack(ctx);

        this.userData.initialize(ctx);
        this.kernelData.initialize(ctx);
        this.stack.initialize(ctx);
    }

    static update(ctx) {
        this.userData.update(ctx);
        this.kernelData.update(ctx);
        this.stack.update(ctx);
    }

    static changeStackRadix(radixStr) {
        const radix = Number.parseInt(radixStr);
        this.stack.changeRadix(radix);
    }

    static changeDataRadix(radixStr) {
        const radix = Number.parseInt(radixStr);
        this.kernelData.changeRadix(radix);
        this.userData.changeRadix(radix);
    }

    static toggleKernelData(showKernelData) {
        if (showKernelData)
            Elements.kernelDataContainer.style.display = null;
        else
            Elements.kernelDataContainer.style.display = 'none';
    }
}

class Memory {
    constructor() {
        this.radix = 16;
        this.lines = [];
        this.content = undefined;
        this.element = undefined;
    }

    /**
     * Initialize `element` and `lines`
     */
    initialize(ctx) {
        this.ctx = ctx;
        this.element.innerHTML = '';
        this.addNewLines();
    }

    /**
     * update the radix for values
     * @param radix new radix
     */
    changeRadix(radix) {
        this.radix = radix;
        for (const line of this.lines)
            for (const word of line.wordList)
                word.valueElement.innerText = word.getValueInnerText()
    }

    /**
     * Check whether a line with the corresponding start address is empty (i.e., all zeros)
     * @param startAddress the start address of the line
     * @returns {boolean}
     */
    isLineEmpty(startAddress) {
        for (let i = startAddress; i < startAddress + 0x10; i += 4)
            if (this.getContent(i) !== 0) return false;
        return true;
    }

    /**
     * Adding new lines to `this.lines`
     */
    addNewLines() {
    }

    /**
     * update the memory values
     */
    update() {
    }

    /**
     * Get content from a certain memory address
     * @param address a memory address
     * @returns {number} the content in the address in int32
     */
    getContent(address) {
        return 0;
    }
}

class DataSegment extends Memory {
    constructor() {
        super();
        this.lineAddresses = new Set();
    }

    addNewLines() {
        for (let i = 0; i < this.content.length / 16; i++) {
            const addr = this.startAddress + i * 0x10;
            if (this.isLineEmpty(addr)) continue;
            this.addLine(addr);
        }
    }

    addLine(addr) {
        const newLine = new MemoryLine(addr, this);
        newLine.updateValues();
        this.element.append(newLine.element);
        this.lines.push(newLine);
        this.lineAddresses.add(addr);
    }

    getContent(addr) {
        return this.content[(addr - this.startAddress) >> 2];
    }
}

class UserData extends DataSegment {
    constructor(ctx) {
        super();
        this.ctx = ctx;
        this.element = Elements.userData;
        this.content = Module.getUserData(this.ctx);
        this.startAddress = 0x10000000;
        
    }

    update(ctx) {
        this.ctx = ctx;
        this.content = Module.getUserData(ctx);

        for (let i = 0; i < this.content.length / 16; i++) {
            const addr = this.startAddress + i * 0x10;
            if (!this.isLineEmpty(addr) && !this.lineAddresses.has(addr))
                this.addLine(addr);
        }

        this.lines.forEach(e => e.updateValues());
    }
}

class KernelData extends DataSegment {
    constructor(ctx) {
        super();
        this.ctx = ctx;
        this.element = Elements.kernelData;
        this.content = Module.getKernelData(this.ctx);
        this.startAddress = 0x90000000;
    }

    update(ctx) {
        this.ctx = ctx;
        this.content = Module.getKernelData(ctx);

        for (let i = 0; i < this.content.length / 16; i++) {
            const addr = this.startAddress + i * 0x10;
            if (!this.isLineEmpty(addr) && !this.lineAddresses.has(addr))
                this.addLine(addr);
        }

        this.lines.forEach(e => e.updateValues());
    }
}

class Stack extends Memory {
    constructor(ctx) {
        super();
        this.ctx = ctx;
        this.content = Module.getStack(this.ctx);
        this.element = Elements.stack;
    }

    update(ctx) {
        if (RegisterUtils.getSP() < this.minLineAddress)
            this.addNewLines(this.minLineAddress);
        this.ctx = ctx;
        this.content = Module.getStack(ctx);
        this.lines.forEach(e => e.updateValues());
    }

    getContent(addr) {
        if (RegisterUtils.getSP() > addr) return undefined;
        const index = this.content.length - (0x80000000 - addr) / 4;
        return this.content[index];
    }

    addNewLines(endAddr = 0x80000000) {
        for (; endAddr >= RegisterUtils.getSP(); endAddr -= 0x10) {
            const newLine = new MemoryLine(endAddr - 0x10, this);
            Elements.stack.prepend(newLine.element);
            this.lines.push(newLine);
        }
        this.minLineAddress = RegisterUtils.getSP() & 0xfffffff0;
    }
}
