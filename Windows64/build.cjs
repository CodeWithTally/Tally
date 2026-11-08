const fs=require('node:fs');const vm=require('node:vm');
const js=fs.readFileSync('dist/tally.js','utf8'),app=fs.readFileSync('dist/app.js','utf8');
let html=fs.readFileSync('dist/index.html','utf8').replace('<script src="tally.js"></script>',()=>'<script>'+js.replace(/<\/script/gi,'<\\/script')+'</script>').replace('<script src="app.js"></script>',()=>'<script>'+app.replace(/<\/script/gi,'<\\/script')+'</script>');
html=html.replace('href="Tally-Setup-0.4.1-x64.exe"', 'href="https://tally-language-lab.candyvole257.chatgpt.site/Tally-Setup-0.4.1-x64.exe"');
html=html.replace('</body>',`<script>document.getElementById('offline').textContent='✓ Offline edition';document.getElementById('offline').disabled=true;</script></body>`);fs.writeFileSync('dist/Tally-Offline.html',html);
const literal=app.slice(app.indexOf('const examples=')+'const examples='.length,app.indexOf('\nconst $='));const examples=vm.runInNewContext(literal);for(const ex of examples)fs.writeFileSync('examples/'+ex.file+'.tally',ex.code+'\n');
console.log('Built offline runner and '+examples.length+' examples.');
