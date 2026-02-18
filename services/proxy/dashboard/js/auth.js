/* Dashboard authentication */
var Dashboard = Dashboard || {};

Dashboard.auth = (function() {
    var loginBtn = null;
    var cookieInput = null;
    var errorDiv = null;
    var browserLoginBtn = null;
    var browserStatusDiv = null;
    var browserStatusText = null;
    var pollInterval = null;
    var pollStartTime = 0;
    var POLL_TIMEOUT_MS = 120000;

    function init() {
        browserLoginBtn = document.getElementById('btn-browser-login');
        browserStatusDiv = document.getElementById('browser-login-status');
        browserStatusText = document.getElementById('browser-status-text');
        errorDiv = document.getElementById('login-error');

        browserLoginBtn.addEventListener('click', doBrowserLogin);
        document.getElementById('btn-logout').addEventListener('click', doLogout);
    }

    /* Raw fetch that does NOT go through the api.js 401 interceptor. */
    function rawPost(path, body) {
        return fetch(path, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(body || {})
        }).then(function(res) {
            return res.json().then(function(data) {
                if (res.ok) return data;
                var msg = (data.error && data.error.message)
                    ? data.error.message : 'Request failed';
                return Promise.reject(new Error(msg));
            });
        });
    }

    function rawGet(path) {
        return fetch(path, {
            method: 'GET',
            headers: { 'Content-Type': 'application/json' }
        }).then(function(res) {
            return res.json().then(function(data) {
                if (res.ok) return data;
                var msg = (data.error && data.error.message)
                    ? data.error.message : 'Request failed';
                return Promise.reject(new Error(msg));
            });
        });
    }

    function doBrowserLogin() {
        hideError();
        browserLoginBtn.disabled = true;
        browserStatusDiv.classList.remove('hidden');
        browserStatusText.textContent = 'Opening browser...';

        rawPost('/api/dashboard/auth/browser-login')
            .then(function() {
                browserStatusText.textContent =
                    'Waiting for login at aichat.uva.nl...';
                pollStartTime = Date.now();
                pollInterval = setInterval(pollBrowserStatus, 2000);
            })
            .catch(function(err) {
                showError(err.message);
                resetBrowserUI();
            });
    }

    function pollBrowserStatus() {
        if (Date.now() - pollStartTime > POLL_TIMEOUT_MS) {
            clearInterval(pollInterval);
            pollInterval = null;
            cancelBrowserLogin();
            showError('Login timed out. Please try again.');
            resetBrowserUI();
            return;
        }

        rawGet('/api/dashboard/auth/browser-status')
            .then(function(data) {
                if (data.status === 'success') {
                    clearInterval(pollInterval);
                    pollInterval = null;
                    Dashboard.api.setToken(data.token);
                    Dashboard.app.showDashboard(data.email, data.name);
                    resetBrowserUI();
                } else if (data.status === 'error') {
                    clearInterval(pollInterval);
                    pollInterval = null;
                    showError(data.message || 'Login failed');
                    resetBrowserUI();
                }
            })
            .catch(function(err) {
                clearInterval(pollInterval);
                pollInterval = null;
                showError(err.message);
                resetBrowserUI();
            });
    }

    function cancelBrowserLogin() {
        rawPost('/api/dashboard/auth/browser-cancel').catch(function() {});
    }

    function resetBrowserUI() {
        browserLoginBtn.disabled = false;
        browserStatusDiv.classList.add('hidden');
    }

    function doLogout() {
        if (pollInterval) {
            clearInterval(pollInterval);
            pollInterval = null;
            cancelBrowserLogin();
        }
        Dashboard.api.post('/api/dashboard/auth/logout', {}).catch(function() {});
        Dashboard.api.clearToken();
        Dashboard.app.showLogin();
    }

    function showError(msg) {
        errorDiv.textContent = msg;
        errorDiv.classList.remove('hidden');
    }

    function hideError() {
        errorDiv.classList.add('hidden');
    }

    return { init: init };
})();
