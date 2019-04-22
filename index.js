const ffi = require('ffi');
const ref = require('ref');

const seaDACObj = ref.types.void
const seaDACObjPtr = ref.refType(seaDACObj);

const SeaDACLib = ffi.Library('./seadac_lib/seadaclib',
  {
    'SeaMaxLinCreate': [seaDACObjPtr, []]
  }
)

const seaDAC = SeaDACLib.SeaMaxLinCreate();

console.log('seaDAC: ', seaDAC);