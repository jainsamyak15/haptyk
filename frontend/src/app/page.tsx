import { AppPanel } from "@/components/AppPanel";
import { Faq } from "@/components/Faq";
import { FinalCta, Footer } from "@/components/Finale";
import { Hero } from "@/components/Hero";
import { Engine } from "@/components/light/Engine";
import { LightFaq } from "@/components/light/Faq";
import { LightFinalCta, LightFooter } from "@/components/light/Finale";
import { LightHero } from "@/components/light/Hero";
import { Marquee } from "@/components/light/Marquee";
import { LightNav } from "@/components/light/Nav";
import { SwitchCards } from "@/components/light/SwitchCards";
import { LightTypeDemo } from "@/components/light/TypeDemo";
import { Nav } from "@/components/Nav";
import { SpecSheet } from "@/components/SpecSheet";
import { SwitchTester } from "@/components/SwitchTester";
import { TypeDemo } from "@/components/TypeDemo";

/* One design per appearance (globals.css shows exactly one). Each has its own ids, so in-page
 * links always land in the design that is showing. */
export default function Home() {
  return (
    <>
      <div className="design-dark">
        <Nav />
        <main>
          <Hero />
          <SwitchTester />
          <TypeDemo />
          <AppPanel />
          <SpecSheet />
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
          <SwitchCards />
          <Engine />
          <LightFaq />
          <LightFinalCta />
        </main>
        <LightFooter />
      </div>
    </>
  );
}
