const ffi = require('ffi');
const ref = require('ref');

// define types
const seaDACObj = ref.types.void
const seaDACObjPtr = ref.refType(seaDACObj);
const ucharPtr = ref.refType('uchar');

const SeaDACLib = ffi.Library('./seadac_lib/seadaclib',
  {
    'SeaMaxLinCreate': [seaDACObjPtr, []],
    'SeaMaxLinOpen': ['int', [seaDACObjPtr, "string"]],
    'SeaDacLinRead': ['int', [seaDACObjPtr, ucharPtr, 'int']],
    'SeaDacLinWrite': ['int', [seaDACObjPtr, ucharPtr, 'int']]
  }
)

let result;

// Create SeaMax Object
const seaDAC = SeaDACLib.SeaMaxLinCreate();

// module string for use with SeaMaxLinOpen(...)
// "sealevel_d2x://product_name"
const portString = 'sealevel_d2x://8111';

// Open the SeaDAC module
result = SeaDACLib.SeaMaxLinOpen(seaDAC, portString);
if (result !== 0) {
  console.log(`ERROR: Open failed, Returned ${result}\n`);
  process.exit(1);
}

// Data for this SeaDAC module is eight bits or one byte.
// the first four bits are read only and the high order four bits
// are used for writing.
let inputData = ref.alloc('uchar', 0x00);

// Read the state of the inputs (lower nibble)
result = SeaDACLib.SeaDacLinRead(seaDAC, inputData, 1);
if (result < 0) {
  console.log(`ERROR: Error reading inputs, Returned ${result}\n`);
  process.exit(1);
} else {
  console.log(`Read Inputs: ${inputData.deref() & 0x0F}`);
}

// Write the value 0xA out to the outputs (upper nibble), effectively 
// turning them on and off in an alternating pattern.
let outputData = ref.alloc('uchar', 0xA0);

console.log('output data length before: ', outputData.length);
console.log('output data before: ', outputData[0]);
console.log('output data before: ', outputData.deref());

result = SeaDACLib.SeaDacLinWrite(seaDAC, outputData, 1);
if (result < 0) {
  console.log(`ERROR: Error writing outputs, Returned ${result}\n`);
  process.exit(1);
} else {
  console.log('output data length after: ', outputData.length);
  console.log('output data after: ', outputData[0]);
  console.log('output data after: ', outputData.deref());
  console.log(`Write Outputs: ${(outputData.deref() >> 4) & 0x0F}`);
}