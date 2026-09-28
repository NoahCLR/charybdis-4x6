const assert = require('node:assert/strict');
const {decode} = require('../../tools/capture-split-diagnostics.cjs');
const p = Buffer.alloc(32);
p[0]=7;p[2]=10;p[3]=1;p[6]=25;p[7]=1;p[8]=8;p[10]=1;p.writeUInt32LE(10000000,11);
assert.deepEqual(decode(0,p),{transactionCount:8,armed:false,frozen:true,durationUs:10000000,activityId:0});
p[4]=2;p[8]=1;p.writeUInt32LE(400,9);p.writeUInt32LE(1,13);p.writeUInt32LE(5600,17);p.writeUInt32LE(250000,21);p.writeUInt32LE(5000,25);
assert.deepEqual(decode(2,p),{id:1,attempts:400,failures:1,attemptedBytes:5600,totalUs:250000,maxUs:5000});
p[5]=3;assert.throws(()=>decode(2,p));p[5]=0;p[8]=2;assert.throws(()=>decode(2,p));assert.throws(()=>decode(2,[]));
console.log('split diagnostic tool decoding tests passed');
