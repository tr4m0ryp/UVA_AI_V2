/* Sidebar toggle, mobile overlay, and navigation */
var Dashboard = Dashboard || {};

Dashboard.sidebar = (function() {
    var sidebar = null;
    var backdrop = null;
    var COLLAPSED_KEY = 'sidebar_collapsed';

    function init() {
        sidebar = document.getElementById('sidebar');
        backdrop = document.getElementById('sidebar-backdrop');

        /* Toggle button */
        document.getElementById('sidebar-toggle').addEventListener('click', toggle);

        /* Backdrop click closes on mobile */
        backdrop.addEventListener('click', closeMobile);

        /* Nav item clicks */
        var items = sidebar.querySelectorAll('.sidebar-item:not(.disabled)');
        for (var i = 0; i < items.length; i++) {
            items[i].addEventListener('click', onNavClick);
        }

        /* Restore collapse state on desktop */
        if (localStorage.getItem(COLLAPSED_KEY) === '1') {
            sidebar.classList.add('collapsed');
        }
    }

    function toggle() {
        var isMobile = window.innerWidth <= 768;
        if (isMobile) {
            sidebar.classList.toggle('open');
            backdrop.classList.toggle('visible');
        } else {
            sidebar.classList.toggle('collapsed');
            var collapsed = sidebar.classList.contains('collapsed');
            localStorage.setItem(COLLAPSED_KEY, collapsed ? '1' : '0');
        }
    }

    function closeMobile() {
        sidebar.classList.remove('open');
        backdrop.classList.remove('visible');
    }

    function onNavClick(e) {
        var view = this.getAttribute('data-view');
        if (view) {
            Dashboard.router.navigate(view);
            closeMobile();
        }
    }

    function setActive(viewName) {
        var items = sidebar.querySelectorAll('.sidebar-item');
        for (var i = 0; i < items.length; i++) {
            items[i].classList.toggle('active',
                items[i].getAttribute('data-view') === viewName);
        }
    }

    function setUser(name) {
        var el = document.getElementById('sidebar-user');
        if (el) el.textContent = name || '';
    }

    return {
        init: init,
        setActive: setActive,
        setUser: setUser,
        toggle: toggle
    };
})();
