const ffi = require('ffi');
const ref = require('ref');

const seaDACObj = ref.types.void;
const seaDACObjPtr = ref.refType(seaDACObj);
const ucharPtr = ref.refType('uchar');

class SeaDAC {
  constructor() {
    this.lib = ffi.Library('./seadac_lib/seadaclib',
      {
        'SeaMaxLinCreate': [seaDACObjPtr, []],
        'SeaMaxLinOpen': ['int', [seaDACObjPtr, "string"]],
        'SeaDacLinRead': ['int', [seaDACObjPtr, ucharPtr, 'int']],
        'SeaDacLinWrite': ['int', [seaDACObjPtr, ucharPtr, 'int']]
      }
    );
    // Create SeaMax Object
    this.seaDAC = this.lib.SeaMaxLinCreate();
    // module string for use with SeaMaxLinOpen(...)
    // "sealevel_d2x://product_name"
    this.port = 'sealevel_d2x://8111';
  }

  open() {
    return new Promise((resolve, reject) => {
      const res = this.lib.SeaMaxLinOpen(this.seaDAC, this.port);
      if (res !== 0) {
        const error = new Error('Could not open device');
        error.cause = 'hardware error';
        error.hardwareErrorCode = ret;
        return reject(error);
      }
      return resolve(res);
    })
  }

  read() {
    return new Promise((resolve, reject) => {
      // Data for this SeaDAC module is eight bits or one byte.
      // the first four bits are read only and the high order four bits
      // are used for writing.
      let inputData = ref.alloc('uchar', 0x00);
      // Read the state of the inputs (lower nibble)
      const res = this.lib.SeaDacLinRead(this.seaDAC, inputData, 1);
      if (res < 0) {
        const error = new Error('Could not read from I/O');
        error.cause = 'hardware error';
        error.hardwareErrorCode = res;
        return reject(error);
      }
      return resolve(inputData.deref() & 0x0F);
    })
  }

  write(value) {
    return new Promise((resolve, reject) => {
      let outputData = ref.alloc('uchar', value);
      const res = this.lib.SeaDacLinWrite(this.seaDAC, outputData, 1);
      if (res < 0) {
        const error = new Error('Could not write to I/O');
        error.cause = 'hardware error';
        error.hardwareErrorCode = res;
        return reject(error);
      }
      return resolve((outputData.deref() >> 4) & 0x0F);
    })
  }
}

const main = async () => {
  let res;
  const seadac = new SeaDAC();
  res = await seadac.open()
    .catch(error => {
      console.log('open error: ', error);
      throw error;
    })
  res = await seadac.write(0xA0)
    .catch(error => {
      console.log('write error: ', error);
      throw error;
    })
  console.log('write returns: ', res);
  res = await seadac.read()
    .catch(error => {
      console.log('read error: ', error);
      throw error;
    })
  console.log('read returns: ', res);

  
}

main();

