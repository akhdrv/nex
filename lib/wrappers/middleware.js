'use strict';

const path = require('path');

function getNativeMiddlewareLoader(nativeMiddlewareInternal) {
    return function (p) {
        return nativeMiddlewareInternal(path.resolve(p));
    }
}

function middlewareWrapper(middleware) {
    if (middleware.length < 4 && middleware.length > 1) {
        function wrapped(req, res, nextObj) {
            function next(str) {
                if (typeof str === 'undefined') {
                    nextObj.next();
                } else if (str === 'route') {
                    nextObj.nextRoute();
                } else if (typeof str === 'object') {
                    nextObj.error(str.toString());
                }
            }

            try {
                middleware(req, res, next);
            } catch (e) {
                nextObj.error(e.toString());
            }
        }

        wrapped.isErrorHandling = false;

        return wrapped;
    }

    if (middleware.length === 4) {
        function wrappedEH(req, res, nextObj, err) {
            function next(str) {
                if (typeof str === 'undefined') {
                    nextObj.next();
                } else if (str === 'route') {
                    nextObj.nextRoute();
                } else if (typeof str === 'object') {
                    nextObj.error(str.message || str.toString());
                }
            }
            try {
                middleware(err, req, res, next);
            } catch (e) {
                nextObj.error(e.toString());
            }
        }

        wrappedEH.isErrorHandling = true;

        return wrappedEH;
    }

    throw new Error('Middleware must receive up to 4 arguments');
}

module.exports = {
    middlewareWrapper,
    getNativeMiddlewareLoader,
};