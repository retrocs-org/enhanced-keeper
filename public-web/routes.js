'use strict';
// Static assets are real files; pages and HTTP endpoints use the PHP entry point.
function kfxRoute(path) {
 if(typeof path!=='string'||!path.startsWith('/')||path.startsWith('//'))throw Error('Invalid internal route');
 return '/index.php?route='+encodeURIComponent(path);
}
function kfxPage() {
 const route=new URLSearchParams(location.search).get('route');
 return route?route.split('?')[0]:location.pathname;
}
