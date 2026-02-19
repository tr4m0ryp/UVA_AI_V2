/* Dashboard API client */
var Dashboard = Dashboard || {};

Dashboard.api = (function() {
    function getToken() {
        return localStorage.getItem('dashboard_token');
    }

    function setToken(token) {
        localStorage.setItem('dashboard_token', token);
    }

    function clearToken() {
        localStorage.removeItem('dashboard_token');
    }

    function request(method, path, body) {
        var headers = { 'Content-Type': 'application/json' };
        var token = getToken();
        if (token) headers['Authorization'] = 'Bearer ' + token;

        var opts = { method: method, headers: headers };
        if (body) opts.body = JSON.stringify(body);

        return fetch(path, opts).then(function(res) {
            if (res.status === 401) {
                clearToken();
                Dashboard.app.showLogin();
                return Promise.reject(new Error('Unauthorized'));
            }
            return res.json().then(function(data) {
                if (res.ok) return data;
                return Promise.reject(new Error(
                    data.error ? data.error.message : 'Request failed'));
            });
        });
    }

    return {
        getToken: getToken,
        setToken: setToken,
        clearToken: clearToken,
        get: function(path) { return request('GET', path); },
        post: function(path, body) { return request('POST', path, body); },
        put: function(path, body) { return request('PUT', path, body); },
        del: function(path) { return request('DELETE', path); }
    };
})();
