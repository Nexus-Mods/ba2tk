let nbind = require('nbind');
let path = require('path');
let bsatk = nbind.init(path.join(__dirname, 'ba2tk')).lib;

module.exports.default = ba2tk.loadBA2;
