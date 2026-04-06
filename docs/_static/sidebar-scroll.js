(() => {
  const STORAGE_KEY = "symusic-docs-sidebar-scroll-top";

  function getSidebarScroll() {
    return document.querySelector(".sidebar-drawer .sidebar-scroll");
  }

  function restoreSidebarScroll() {
    const sidebarScroll = getSidebarScroll();
    if (!sidebarScroll) {
      return;
    }
    const savedValue = sessionStorage.getItem(STORAGE_KEY);
    if (savedValue === null) {
      return;
    }
    const scrollTop = Number.parseInt(savedValue, 10);
    if (Number.isNaN(scrollTop)) {
      return;
    }

    // Furo finishes laying out the sidebar after initial DOM parsing, so defer
    // the restoration until the next frame to avoid snapping back to the top.
    requestAnimationFrame(() => {
      sidebarScroll.scrollTop = scrollTop;
    });
  }

  function persistSidebarScroll() {
    const sidebarScroll = getSidebarScroll();
    if (!sidebarScroll) {
      return;
    }
    sessionStorage.setItem(STORAGE_KEY, String(sidebarScroll.scrollTop));
  }

  document.addEventListener("DOMContentLoaded", () => {
    const sidebarScroll = getSidebarScroll();
    if (!sidebarScroll) {
      return;
    }

    restoreSidebarScroll();
    sidebarScroll.addEventListener("scroll", persistSidebarScroll, { passive: true });
  });

  window.addEventListener("pagehide", persistSidebarScroll);
})();
