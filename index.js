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

const sleep = s => new Promise(resolve => setTimeout(resolve, 1000*s));

const main = async () => {
  let result, inputData, outputData;

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
  inputData = ref.alloc('uchar', 0x00);

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
  outputData = ref.alloc('uchar', 0xA0);
  result = SeaDACLib.SeaDacLinWrite(seaDAC, outputData, 1);
  if (result < 0) {
    process.exit(1);
  } else {
    console.log(`Write Outputs: ${(outputData.deref() >> 4) & 0x0F}`);
  }

  await sleep(1);

  inputData = ref.alloc('uchar', 0x00);
  result = SeaDACLib.SeaDacLinRead(seaDAC, inputData, 1);
  if (result < 0) {
    console.log(`ERROR: Error reading inputs, Returned ${result}\n`);
    process.exit(1);
  } else {
    console.log(`Read Inputs: ${inputData.deref() & 0x0F}`);
    console.log(`Write Outputs: ${(outputData.deref() >> 4) & 0x0F}`);
  }
}

main();

