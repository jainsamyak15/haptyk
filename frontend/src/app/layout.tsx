import { Analytics } from "@vercel/analytics/next";
import type { Metadata, Viewport } from "next";
import { Bricolage_Grotesque, Geist, Geist_Mono, Unbounded } from "next/font/google";

import { THEME_KEY } from "@/lib/theme";

import "./globals.css";

/* Self-hosted at build time by next/font: visitors' browsers never call Google. Unbounded is
 * the dark design's display face, Bricolage Grotesque the light design's. */
const unbounded = Unbounded({ variable: "--font-unbounded", subsets: ["latin"] });
const bricolage = Bricolage_Grotesque({ variable: "--font-bricolage", subsets: ["latin"] });
const geistSans = Geist({ variable: "--font-geist-sans", subsets: ["latin"] });
const geistMono = Geist_Mono({ variable: "--font-geist-mono", subsets: ["latin"] });

const description =
  "Real mechanical keyboard sounds for your MacBook. 25 switch packs, a tiny menu bar app, and nothing running while you're not typing.";

export const metadata: Metadata = {
  title: "Keyfeel — mechanical keyboard sounds for your Mac",
  description,
  openGraph: {
    title: "Keyfeel — the keyboard sound your MacBook forgot",
    description,
    type: "website",
  },
};

export const viewport: Viewport = {
  themeColor: [
    { media: "(prefers-color-scheme: dark)", color: "#121211" },
    { media: "(prefers-color-scheme: light)", color: "#f6f4ef" },
  ],
};

/* Reapplies a visitor's chosen appearance before first paint, so the other design never
 * flashes. Storage can be unavailable (private windows): then the system setting applies. */
const applySavedTheme = `try{var t=localStorage.getItem(${JSON.stringify(THEME_KEY)});if(t==="light"||t==="dark")document.documentElement.dataset.theme=t}catch(e){}`;

export default function RootLayout({ children }: LayoutProps<"/">) {
  return (
    <html
      lang="en"
      suppressHydrationWarning // The script below may set data-theme before React hydrates.
      className={`${unbounded.variable} ${bricolage.variable} ${geistSans.variable} ${geistMono.variable}`}
    >
      <head>
        <script dangerouslySetInnerHTML={{ __html: applySavedTheme }} />
      </head>
      <body className="min-h-full">
        {children}
        {/* Vercel Web Analytics (ADR-0027): cookieless page views, served from this site. Every
            Download button leads to /download, so its page views count the downloads. */}
        <Analytics />
      </body>
    </html>
  );
}
