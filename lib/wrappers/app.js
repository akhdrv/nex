'use strict';

const nexpressCore = require('../../build/Release/nexpress');
const Router = require('./router');
const {getNativeMiddlewareLoader} = require('./middleware');

function Application(internalApplicationInstance) {
    Router.call(this, internalApplicationInstance);
}

Application.prototype = Object.create(Router.prototype);

Application.prototype.__isNexpressRouter = false;
Application.prototype.__isNexpressApp = true;

Application.prototype.listen = function(ip, port) {
    this.__instance.listen(ip, port);
}

Application.prototype.close = function() {
    this.__instance.close();
}

Application.prototype.set = function(settingName, settingValue) {
    if (typeof settingName !== 'string')
        throw new Error('Setting name must be string');

    this.__instance.set(settingName, settingValue);
}

function createApplication() {
    return new Application(nexpressCore())
}

createApplication.Router = function() {
    return new Router(nexpressCore.Router());
};

createApplication.NativeMiddleware = getNativeMiddlewareLoader(nexpressCore.NativeMiddlewareWrap);

module.exports = createApplication;
