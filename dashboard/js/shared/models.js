/* Model list fetcher */
var Dashboard = Dashboard || {};

Dashboard.models = (function() {
    var cache = null;

    function fetch() {
        if (cache) return Promise.resolve(cache);
        return Dashboard.api.get('/v1/models').then(function(data) {
            cache = data.data || [];
            return cache;
        });
    }

    function populateSelect(selectEl, selected) {
        return fetch().then(function(models) {
            selectEl.innerHTML = '';
            models.forEach(function(m) {
                var opt = document.createElement('option');
                opt.value = m.id;
                opt.textContent = m.id;
                if (m.id === selected) opt.selected = true;
                selectEl.appendChild(opt);
            });
        });
    }

    return {
        fetch: fetch,
        populateSelect: populateSelect
    };
})();
