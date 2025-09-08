// Tiny enhancements
const year = document.getElementById('year');
if (year) { year.textContent = new Date().getFullYear(); }

// Smooth scroll for in-page anchors
document.querySelectorAll('a[href^="#"]').forEach(a => {
  a.addEventListener('click', e => {
    const id = a.getAttribute('href').slice(1);
    const el = document.getElementById(id);
    if (el) {
      e.preventDefault();
      el.scrollIntoView({ behavior: 'smooth', block: 'start' });
    }
  });
});

// Entrance reveal on scroll
const reveal = new IntersectionObserver((entries) => {
  entries.forEach(e => {
    if (e.isIntersecting) {
      e.target.animate(
        [
          { opacity: 0, transform: 'translateY(12px)' },
          { opacity: 1, transform: 'translateY(0)' },
        ],
        { duration: 500, easing: 'cubic-bezier(.2,.6,.2,1)', fill: 'forwards' }
      );
      reveal.unobserve(e.target);
    }
  });
}, { threshold: 0.1 });

document.querySelectorAll('.card, .hero-card, .hero-img').forEach(el => reveal.observe(el));

// -------------------
// API Demo Fetch
// -------------------
const apiBox = document.getElementById('apiResult');
if (apiBox) {
  fetch('/api/getData')
    .then(res => res.json())
    .then(data => {
      apiBox.textContent = data.message;
    })
    .catch(err => {
      apiBox.textContent = "Error fetching API: " + err;
    });
}
