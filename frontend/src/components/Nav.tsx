import { DownloadButton, Logo } from "./Bits";
import { ThemeToggle } from "./ThemeToggle";

const links = [
  { href: "#switches", label: "Switches" },
  { href: "#try", label: "Try it" },
  { href: "#specs", label: "Specs" },
  { href: "#faq", label: "FAQ" },
];

export function Nav() {
  return (
    <header className="sticky top-0 z-50 rule-b bg-page/85 backdrop-blur-md">
      <nav className="mx-auto flex h-[76px] max-w-[1280px] items-center justify-between gap-6 px-5 sm:px-8">
        <div className="flex items-center gap-10">
          <a href="#top" aria-label="Keyfeel home">
            <Logo />
          </a>
          <ul className="hidden items-center gap-8 text-[15px] font-medium text-ink-dim md:flex">
            {links.map((l) => (
              <li key={l.href}>
                <a href={l.href} className="transition-colors hover:text-ink">
                  {l.label}
                </a>
              </li>
            ))}
          </ul>
        </div>
        <div className="flex items-center gap-3">
          <ThemeToggle className="text-ink-dim edge hover:bg-raised hover:text-ink" />
          <DownloadButton size="sm" label="Download" />
        </div>
      </nav>
    </header>
  );
}
