(async function () {
  const config = document.getElementById('mermaid-config');
  if (!config || !window.mermaid) return;
  const diagrams = document.querySelectorAll('.mermaid');
  if (!diagrams.length) return;

  mermaid.initialize(Object.assign({}, JSON.parse(config.textContent), {
    startOnLoad: false
  }));
  for (const diagram of diagrams) {
    const source = diagram.textContent;
    try {
      await mermaid.run({nodes: [diagram]});
      const svg = diagram.querySelector('svg');
      if (svg && svg.viewBox.baseVal.width) {
        // Keep labels readable on phones; the frame scrolls horizontally.
        svg.style.minWidth = Math.ceil(svg.viewBox.baseVal.width) + 'px';
        diagram.tabIndex = 0;
        diagram.setAttribute('role', 'region');
        diagram.setAttribute('aria-label', '图表（可滚动）');
      }
      diagram.dataset.renderState = 'ready';
    } catch (error) {
      // A malformed legacy diagram must not prevent later diagrams rendering.
      diagram.textContent = source;
      diagram.dataset.renderState = 'error';
      console.warn('Could not render Mermaid diagram:', error);
    }
  }
})();
