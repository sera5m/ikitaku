export function CrtOverlay() {
  return (
    <div className="pointer-events-none fixed inset-0 z-30" aria-hidden>
      <div className="crt-bloom absolute inset-0" />
      <div className="crt-scan absolute inset-0" />
      <div className="crt-vignette absolute inset-0" />
    </div>
  );
}
