/** Where the demo's sounds come from, for both designs' footers. `linkClass` styles the links. */
export function Credits({ className = "", linkClass = "" }: { className?: string; linkClass?: string }) {
  const link = `underline underline-offset-2 ${linkClass}`;
  return (
    <p className={`max-w-[560px] text-[14px] leading-relaxed ${className}`}>
      Demo sounds on this page: NK Cream, Cherry MX Blue and Topre from{" "}
      <a className={link} href="https://github.com/hainguyents13/mechvibes">
        Mechvibes
      </a>{" "}
      (MIT, © 2021 Hai Nguyen,{" "}
      <a className={link} href="/sounds/LICENSE-mechvibes.txt">
        licence
      </a>
      ), and Holy Panda from{" "}
      <a className={link} href="https://github.com/tplai/kbsim">
        kbsim
      </a>{" "}
      (MIT, © Thomas Lai,{" "}
      <a className={link} href="/sounds/LICENSE-kbsim.txt">
        licence
      </a>
      ).
    </p>
  );
}
