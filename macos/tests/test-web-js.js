// No browser or third-party package required: exercise the explorer's redaction boundary.
const vm=require('node:vm'),fs=require('node:fs'),assert=require('node:assert/strict');
const element={value:'',addEventListener(){},textContent:'',disabled:false};
const context=vm.createContext({document:{getElementById:()=>element},fetch:async()=>({ok:true,json:async()=>[]})});
vm.runInContext(fs.readFileSync('public-web/api-browser.js','utf8'),context);
const value=vm.runInContext(`redact({id:'visible',csrf:'private',token:'private',codes:['private'],user_code:'private',nested:[{password:'private',name:'Keeper'}]})`,context);
assert.equal(value.id,'visible');assert.equal(value.token,'[redacted]');assert.equal(value.csrf,'[redacted]');assert.equal(value.codes,'[redacted]');assert.equal(value.user_code,'[redacted]');assert.equal(value.nested[0].password,'[redacted]');assert.equal(value.nested[0].name,'Keeper');
console.log('PASS: API explorer redacts credentials and recovery/pairing codes recursively while retaining public metadata.');
