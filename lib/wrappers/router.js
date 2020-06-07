'use strict';

const {middlewareWrapper} = require('./middleware');
const {pathToRegexp} = require('path-to-regexp');


function Router(internalRouterInstance) {
    this.__instance = internalRouterInstance;
}

Router.prototype.__isNexpressRouter = true;

Router.prototype.use = function () {
    const {path, keys} = getRegexAndKeys(arguments[0]);

    let i = 0;
    let middlewares = [];

    if (path) {
        i = 1;
    }

    for (; i < arguments.length; ++i) {
        middlewares.push(wrapIfNeeded(arguments[i]));
    }

    this.__instance.use(null, true, path, keys, ...middlewares);

    return this;
}

const methods = [
    'ACL', 'BIND', 'CHECKOUT', 'CONNECT',
    'COPY', 'DELETE', 'GET', 'HEAD',
    'LINK', 'LOCK', 'M-SEARCH', 'MERGE',
    'MKACTIVITY', 'MKCALENDAR', 'MKCOL', 'MOVE',
    'NOTIFY', 'OPTIONS', 'PATCH', 'POST',
    'PROPFIND', 'PROPPATCH', 'PURGE', 'PUT',
    'REBIND', 'REPORT', 'SEARCH', 'SOURCE',
    'SUBSCRIBE', 'TRACE', 'UNBIND', 'UNLINK',
    'UNLOCK', 'UNSUBSCRIBE'
];

for (let method of methods) {
    Router.prototype[method.toLowerCase()] = getMethod(method);
}


function getRegexAndKeys(p) {
    if (typeof p !== 'string') {
        return {
            path: null,
            keys: null,
        };
    }

    let keys = [];

    const path = pathToRegexp(p, keys, {strict: true})
        .toString()
        .replace(/^\/\^/, '')
        .replace(/\$[^$]+$/, '');

    keys = keys.map(function ({name}) {
        return name;
    });

    if (!keys.length) {
        keys = null;
    }

    return {path, keys};
}

function getMethod(method) {
    return function() {
        const {path, keys} = getRegexAndKeys(arguments[0]);

        if (path === null) {
            return this;
        }

        let middlewares = [];

        for (let i = 1; i < arguments.length; ++i) {
            middlewares.push(wrapIfNeeded(arguments[i]));
        }

        this.__instance.use(method, false, path, keys, ...middlewares);
    }
}

function wrapIfNeeded(middleware) {
    if (
        middleware.__isNexpressRouter
        || middleware.__isNexpressApp
        || middleware.__isNexpressNativeMiddleware
    ) {
        return middleware;
    }

    return middlewareWrapper(middleware);
}


module.exports = Router;