// ── Sidebar group toggle ─────────────────────────────────
function toggleGroup(el) {
  el.parentElement.classList.toggle('collapsed');
}

// ── Sidebar search filter ────────────────────────────────
function filterNav(query) {
  var q = query.toLowerCase();
  var groups = document.querySelectorAll('.nav-group');
  groups.forEach(function(g) {
    var items = g.querySelectorAll('.nav-item');
    var anyVisible = false;
    items.forEach(function(item) {
      var text = item.textContent.toLowerCase();
      var match = !q || text.indexOf(q) !== -1;
      item.style.display = match ? '' : 'none';
      if (match) anyVisible = true;
    });
    g.style.display = anyVisible ? '' : 'none';
    if (q && anyVisible) g.classList.remove('collapsed');
  });
}

// ── Active link tracking on scroll ───────────────────────
(function() {
  var sections = [];
  var links = {};

  function refresh() {
    sections = [];
    links = {};
    document.querySelectorAll('.doc-section').forEach(function(s) {
      sections.push(s);
      var a = document.querySelector('#sidebar-nav a[href="#' + s.id + '"]');
      if (a) links[s.id] = a;
    });
  }

  function onScroll() {
    var scrollY = window.scrollY + 80;
    var current = null;
    for (var i = sections.length - 1; i >= 0; i--) {
      if (sections[i].offsetTop <= scrollY) { current = sections[i].id; break; }
    }
    Object.values(links).forEach(function(a) { a.classList.remove('active'); });
    if (current && links[current]) {
      links[current].classList.add('active');
      // Scroll sidebar to show active link
      var rect = links[current].getBoundingClientRect();
      var nav = document.getElementById('sidebar-nav');
      var navRect = nav.getBoundingClientRect();
      if (rect.top < navRect.top || rect.bottom > navRect.bottom) {
        links[current].scrollIntoView({ block: 'center', behavior: 'smooth' });
      }
    }
  }

  // Scroll-to-top button
  function checkScrollTop() {
    var btn = document.getElementById('scroll-top');
    if (window.scrollY > 400) { btn.style.display = 'flex'; }
    else { btn.style.display = 'none'; }
  }

  window.addEventListener('scroll', function() { onScroll(); checkScrollTop(); });
  // Refresh after content is loaded
  setTimeout(refresh, 100);
  // Also refresh when hash changes
  window.addEventListener('hashchange', function() { setTimeout(refresh, 50); });
})();
