class RegisterUtils {
    static init(ctx = 0) {
        this.radix = 16;

        const generalRegNames = [
            "r0", "at", "v0", "v1", "a0", "a1", "a2", "a3",
            "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
            "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7",
            "t8", "t9", "k0", "k1", "gp", "sp", "s8", "ra"]
            .map((name, index) => `R${index} (${name})`);
        const specialRegNames = ["PC", "EPC", "Cause", "BadVAddr", "Status", "HI", "LO", "FIR", "FCSR"];
        const floatRegNames = Array(32).fill(0).map((_, i) => `FG${i}`);
        const doubleRegNames = Array(16).fill(0).map((_, i) => `FP${i}`);

        this.specialRegVals = Loader.module().getSpecialRegVals(ctx);
        this.generalRegVals = Loader.module().getGeneralRegVals(ctx);
        this.floatRegVals = Loader.module().getFloatRegVals(ctx);
        this.doubleRegVals = Loader.module().getDoubleRegVals(ctx);

        this.generalRegs = generalRegNames.map(name => new Register(name));
        this.specialRegs = specialRegNames.map(name => new Register(name));
        this.floatRegs = floatRegNames.map(name => new FloatRegister(name));
        this.doubleRegs = doubleRegNames.map(name => new FloatRegister(name));

        this.initElement();
        this.update(ctx);
    }

    static initElement() {
        Elements.generalReg.innerHTML = "";
        Elements.specialReg.innerHTML = "";
        Elements.floatReg.innerHTML = "";
        Elements.doubleReg.innerHTML = "";

        this.generalRegs.forEach(e => Elements.generalReg.appendChild(e.element));
        this.specialRegs.forEach(e => Elements.specialReg.appendChild(e.element));
        this.floatRegs.forEach(e => Elements.floatReg.appendChild(e.element));
        this.doubleRegs.forEach(e => Elements.doubleReg.appendChild(e.element));
    }

    // original update()
    // static update() {
    //     // values in special registers needs to be refreshed
    //     this.specialRegVals = Loader.module().getSpecialRegVals(0);
    //     this.specialRegs.forEach((reg, i) => reg.updateValue(this.specialRegVals[i]));

    //     this.generalRegs.forEach((reg, i) => reg.updateValue(this.generalRegVals[i]));
    //     this.floatRegs.forEach((reg, i) => reg.updateValue(this.floatRegVals[i]));
    //     this.doubleRegs.forEach((reg, i) => reg.updateValue(this.doubleRegVals[i]));
    // }

    static update(ctx) {
        // values in special registers needs to be refreshed
        this.specialRegVals = Loader.module().getSpecialRegVals(ctx);

        this.specialRegs.forEach((reg, i) => reg.updateValue(this.specialRegVals[i]));
        this.generalRegs.forEach((reg, i) => reg.updateValue(Loader.module().getGeneralRegVals(ctx)[i]));
        this.floatRegs.forEach((reg, i) => reg.updateValue(Loader.module().getFloatRegVals(ctx)[i]));
        this.doubleRegs.forEach((reg, i) => reg.updateValue(Loader.module().getDoubleRegVals(ctx)[i]));
    }

    static changeRadix(radix) {
        this.radix = Number.parseInt(radix);
        this.generalRegs.forEach(e => e.valueElement.innerText = e.formatValue());
        this.specialRegs.forEach(e => e.valueElement.innerText = e.formatValue());
    }

    static getSP() {
        return this.generalRegVals[29];
    }

    static getPC() {
        return this.specialRegVals[0];
    }
}

class Register {
    constructor(name) {
        this.name = name.padEnd(8);
        this.value = undefined;

        // init element
        this.element = document.createElement("div");

        const nameElement = document.createElement('span');
        nameElement.classList.add('hljs-string');
        nameElement.innerText = this.name;
        this.element.appendChild(nameElement);

        this.element.appendChild(document.createTextNode(' = '));

        this.valueElement = document.createElement('span');
        this.valueElement.classList.add("hljs-number");
        this.element.appendChild(this.valueElement);
    }

    updateValue(newValue) {
        if (this.value === newValue) {
            this.valueElement.classList.remove('highlight');
            return;
        }

        if (this.value !== undefined)
            this.valueElement.classList.add('highlight');

        this.value = newValue;
        this.valueElement.innerText = this.formatValue();
    }

    formatValue() {
        switch (RegisterUtils.radix) {
            case 2:
                const str = this.value.toString(RegisterUtils.radix).padStart(32, '0');
                return `\n${str.substr(0, 8)} ${str.substr(8, 8)}\n${str.substr(16, 8)} ${str.substr(24, 8)}`;
            case 16:
                return this.value.toString(RegisterUtils.radix).padStart(8, '0');
            default:
                return this.value.toString(RegisterUtils.radix);
        }
    }
}

class FloatRegister extends Register {
    formatValue() {
        return this.value.toPrecision(6);
    }
}
