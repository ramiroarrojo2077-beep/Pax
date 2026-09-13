#include "PaxTouchLayout.h"
#include <cstdio>
#include <vector>
#include <string>

static bool Overlaps(const FBox2D& A, const FBox2D& B)
{
    return !(A.Max.X <= B.Min.X || B.Max.X <= A.Min.X ||
             A.Max.Y <= B.Min.Y || B.Max.Y <= A.Min.Y);
}

static int Failures = 0;
static void Check(bool Condition, const std::string& What)
{
    if (!Condition) { printf("    FALLO: %s\n", What.c_str()); ++Failures; }
}

int main()
{
    // Resoluciones reales: móvil 16:9, dos formatos alargados de 2024,
    // tableta 16:10 y un dispositivo de gama baja.
    const std::vector<std::pair<int,int>> Screens = {
        {1920, 1080}, {2400, 1080}, {2340, 1080}, {2560, 1600}, {1280, 720}, {3200, 1440}
    };

    const char* Names[] = {"DRS","ERS","MIX","CAM","REC","SH+","SH-"};

    for (auto [W, H] : Screens)
    {
        printf("  %dx%d\n", W, H);
        FPaxTouchLayout L;
        L.Build(FVector2D(W, H));

        Check(L.Matches(FVector2D(W, H)), "Matches debe reconocer su propio tamaño");
        Check(!L.Matches(FVector2D(W + 10, H)), "Matches no debe aceptar otro tamaño");

        std::vector<std::pair<std::string, FBox2D>> Rects;
        Rects.push_back({"GAS", L.ThrottlePedal});
        Rects.push_back({"FRENO", L.BrakePedal});
        for (int i = 0; i < PaxNumTouchButtons; ++i)
            Rects.push_back({Names[i], L.Buttons[i]});

        for (auto& [Name, R] : Rects)
        {
            Check(R.Min.X >= 0 && R.Min.Y >= 0 && R.Max.X <= W && R.Max.Y <= H,
                  Name + " se sale de la pantalla");
            Check(R.Max.X > R.Min.X && R.Max.Y > R.Min.Y, Name + " tiene área nula");

            // Un objetivo táctil por debajo de ~7 mm es difícil de acertar.
            // En una pantalla de 1080 de alto y unas 6", 7 mm ≈ 48 px.
            const double MinSide = 48.0 * (H / 1080.0);
            Check(R.GetSize().X >= MinSide * 0.9 && R.GetSize().Y >= MinSide * 0.9,
                  Name + " es demasiado pequeño para un dedo");
        }

        for (size_t i = 0; i < Rects.size(); ++i)
            for (size_t j = i + 1; j < Rects.size(); ++j)
                Check(!Overlaps(Rects[i].second, Rects[j].second),
                      Rects[i].first + " se solapa con " + Rects[j].first);

        // El pulgar izquierdo no debe poder pisar nada del lado derecho.
        for (auto& [Name, R] : Rects)
            Check(!Overlaps(L.SteerArea, R), "la zona de volante invade " + Name);

        Check(L.SteerRadius > 0.0f, "radio de volante nulo");

        // En móvil la HUD recoloca los paneles de neumáticos y energía en la
        // columna izquierda (ver APaxHUD::DrawTyres / DrawEnergy). El pulgar
        // del volante no debe poder caer encima de ellos.
        const double UI = L.Scale;
        const FBox2D LeftColumn(FVector2D(0.0, 0.0),
                                FVector2D(40.0 * UI + 260.0 * UI, 40.0 * UI + 300.0 * UI + 150.0 * UI));
        Check(!Overlaps(L.SteerArea, LeftColumn),
              "la zona de volante invade los paneles de la columna izquierda");
        printf("    escala %.2f, volante %.0f px, gas %.0fx%.0f px\n",
               L.Scale, L.SteerRadius, L.ThrottlePedal.GetSize().X, L.ThrottlePedal.GetSize().Y);
    }

    // El orden de la fila debe dejar el DRS pegado al borde derecho: es el
    // botón que se pulsa en carrera sin mirar.
    FPaxTouchLayout L; L.Build(FVector2D(2400, 1080));
    const FBox2D& Drs = L.GetButton(EPaxTouchButton::DRS);
    for (int i = 0; i < PaxNumTouchButtons; ++i)
    {
        if (static_cast<EPaxTouchButton>(i) == EPaxTouchButton::DRS) continue;
        if (static_cast<EPaxTouchButton>(i) == EPaxTouchButton::ShiftUp) continue;
        if (static_cast<EPaxTouchButton>(i) == EPaxTouchButton::ShiftDown) continue;
        Check(L.Buttons[i].Max.X <= Drs.Max.X, "el DRS debe ser el más a la derecha de su fila");
    }
    Check(Drs.GetSize().X > L.GetButton(EPaxTouchButton::ERS).GetSize().X,
          "el DRS debe ser más ancho que el resto");

    printf(Failures ? "\n%d FALLO(S)\n" : "\nTODO OK\n", Failures);
    return Failures ? 1 : 0;
}
