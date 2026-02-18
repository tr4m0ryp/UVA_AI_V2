/* Hash-based view router with mount/unmount lifecycle */
var Dashboard = Dashboard || {};

Dashboard.router = (function() {
    var views = {};
    var currentView = null;
    var defaultView = 'overview';

    function register(name, handlers) {
        views[name] = {
            mount: handlers.mount || function() {},
            unmount: handlers.unmount || function() {}
        };
    }

    function navigate(name) {
        if (!views[name]) return;
        if (currentView === name) return;

        /* Unmount current view */
        if (currentView && views[currentView]) {
            views[currentView].unmount();
            var oldEl = document.getElementById('view-' + currentView);
            if (oldEl) oldEl.classList.add('hidden');
        }

        /* Mount new view */
        currentView = name;
        var newEl = document.getElementById('view-' + name);
        if (newEl) newEl.classList.remove('hidden');
        views[name].mount();

        /* Update hash without triggering hashchange */
        history.replaceState(null, '', '#' + name);

        /* Sync sidebar highlight */
        if (Dashboard.sidebar) {
            Dashboard.sidebar.setActive(name);
        }
    }

    function onHashChange() {
        var hash = location.hash.replace('#', '') || defaultView;
        if (views[hash]) {
            navigate(hash);
        } else {
            navigate(defaultView);
        }
    }

    function init() {
        window.addEventListener('hashchange', onHashChange);
    }

    function start() {
        var hash = location.hash.replace('#', '') || defaultView;
        if (views[hash]) {
            navigate(hash);
        } else {
            navigate(defaultView);
        }
    }

    function getCurrent() {
        return currentView;
    }

    function reset() {
        currentView = null;
    }

    return {
        register: register,
        navigate: navigate,
        init: init,
        start: start,
        getCurrent: getCurrent,
        reset: reset
    };
})();
