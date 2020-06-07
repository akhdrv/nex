var nexpress = require('../lib');

var app = nexpress();

app.set('keepAlive', true);

var apiRouter = nexpress.Router();

apiRouter.post('/echo', function (req, res, next) {
    req.rawBody = '';

    req.on('data', function(chunk) {
        req.rawBody += chunk;
    });

    req.on('end', function() {
        next();
    });
}, function (req, res) {
    res.send(req.rawBody);
});

app.use('/api', apiRouter);

app.get('/form', function(req, res) {
    res.set('Content-Type', 'text/html; charset=utf-8');
    res.send(`<html><body>
<script type="application/javascript">
function sendToEcho() {
    var text = document.getElementById('textField1').value;
    fetch('/api/echo', {method: 'POST', body: text})
        .then(res => res.text())
        .then(text => {
           document.getElementById('resultField').innerText = text;
        });
}
</script>
<input type="text" id="textField1" value="12345" />
</br>
<button onclick="sendToEcho()">Echo!</button>
</br>
<h1>Result:</h1>
</br>
<div style="color:red;" id="resultField" />
</body>
</html>`);
});

var nativeMiddleware =
    nexpress.NativeMiddleware('./examples/nativeMiddleware/build/lib/native_example.so');

app.get('/native', nativeMiddleware);

app.use('/check', function (req, res, next) {
    res.write('Always Handled!\n');
    next();
});

app.get('/check/check-use', function (req, res, next) {
    res.write('Handled after use-registered middleware.\n');
    res.end();
});

app.get('/', function (req, res, next) {
    res.write('Handled by a middleware!\n');
    next();
}, function (req, res) {
    res.write('Handled by next middleware!\n');
    res.end();
});

app.listen('127.0.0.1', 8888);