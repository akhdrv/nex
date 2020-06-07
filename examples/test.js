var nexpress = require('../lib');

var app = nexpress();

app.get('/', function (req,res) {
    res.send('Hello, world');
});

app.listen('127.0.0.1', 8888);
