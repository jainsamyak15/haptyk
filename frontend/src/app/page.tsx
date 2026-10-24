import { Engine } from "@/components/Engine";
import { Faq } from "@/components/Faq";
import { FinalCta, Footer } from "@/components/Finale";
import { Hero } from "@/components/Hero";
import { Engine as LightEngine } from "@/components/light/Engine";
import { LightFaq } from "@/components/light/Faq";
import { LightFinalCta, LightFooter } from "@/components/light/Finale";
import { LightHero } from "@/components/light/Hero";
import { Marquee } from "@/components/light/Marquee";
import { LightNav } from "@/components/light/Nav";
import { SwitchCards as LightSwitchCards } from "@/components/light/SwitchCards";
import { LightTypeDemo } from "@/components/light/TypeDemo";
import { Nav } from "@/components/Nav";
import { PackBand } from "@/components/PackBand";
import { SwitchCards } from "@/components/SwitchCards";
import { TypeDemo } from "@/components/TypeDemo";

/* One design per appearance (globals.css shows exactly one). Both say the same thing, section
 * for section, from src/lib/content.ts; only the look differs. Each has its own ids, so in-page
 * links always land in the design that is showing. */
export default function Home() {
  return (
    <>
      <div className="design-dark">
        <Nav />
        <main>
          <Hero />
          <PackBand />
          <TypeDemo />
          <SwitchCards />
          <Engine />
          <Faq />
          <FinalCta />
        </main>
        <Footer />
      </div>

      <div className="design-light">
        <LightNav />
        <main>
          <LightHero />
          <Marquee />
          <LightTypeDemo />
          <LightSwitchCards />
          <LightEngine />
          <LightFaq />
          <LightFinalCta />
        </main>
        <LightFooter />
      </div>
    </>
  );
}
