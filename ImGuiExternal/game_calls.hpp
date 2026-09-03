#pragma once
#include "include.h"
#include <cmath>
#include <cstring>

/* ---------------------------------------------------------------------------
 *  Llamadas a funciones nativas del juego.
 *
 *  Solo se expone lo estrictamente necesario para mover la mira:
 *      APlayerController::AddPitchInput(float)
 *      APlayerController::AddYawInput(float)
 *
 *  POR QUE ESTO Y NO ESCRIBIR ControlRotation
 *  ------------------------------------------
 *  ControlRotation es un RESULTADO, no una entrada. Cada tick el juego ejecuta
 *  UpdateRotation() y lo recalcula a partir de RotationInput, machacando lo que
 *  hubieramos escrito. Por eso el aimbot que escribia ControlRotation "pelea"
 *  con el juego: escribe un valor que el siguiente tick descarta.
 *  AddYawInput/AddPitchInput alimentan RotationInput, que es justo la entrada
 *  que el juego espera, y la rotacion sale integrada en su propio ciclo.
 *
 *  SEGURIDAD
 *  ---------
 *  - Esto es 100% user-mode: NO puede provocar un BSOD. El peor caso posible es
 *    cerrar el proceso del juego.
 *  - Antes de llamar nada se verifica la FIRMA de bytes de la funcion. Si el
 *    juego se actualiza y el RVA pasa a apuntar a otro sitio, la firma no
 *    coincide y no se llama: se cae al metodo de escritura directa.
 *  - Se comprueba que la direccion esta dentro del modulo del juego y en una
 *    pagina ejecutable.
 *  - La llamada va envuelta en SEH.
 *  - El valor pasado va acotado, para que un calculo erroneo no pueda producir
 *    un giro absurdo.
 *
 *  SOBRE EL HILO
 *  -------------
 *  El overlay corre en su propio hilo. Estas dos funciones son de las pocas del
 *  motor que se pueden llamar desde fuera del game thread con riesgo bajo:
 *  consultan un flag, leen un float de configuracion y suman a un double. No
 *  reasignan punteros, no tocan contenedores ni el scene graph. Lo peor que
 *  puede pasar es una carrera con el tick del juego que haga que se pierda un
 *  incremento, lo cual se traduce en un microtiron, no en corrupcion.
 *
 *  Esto NO vale para funciones cualquiera. AController::SetControlRotation, por
 *  ejemplo, puede acabar llamando a RootComponent->SetWorldRotation y tocar el
 *  scene graph: esa SI hay que ejecutarla desde el game thread.
 * ------------------------------------------------------------------------- */

namespace GameCalls {

	/* RVAs verificados sobre este binario (ver REVERSE_ENGINEERING.md §9.1).
	 * Las tres funciones son contiguas y miden 0x8C bytes cada una. */
	inline constexpr DWORD64 kRVA_AddPitchInput = 0x3CB80F0;   /* escribe 0x528 */
	inline constexpr DWORD64 kRVA_AddRollInput  = 0x3CB8180;   /* escribe 0x538 */
	inline constexpr DWORD64 kRVA_AddYawInput   = 0x3CB8300;   /* escribe 0x530 */

	using AddInputFn = void(__fastcall*)(void* playerController, float value);

	struct Resolved {
		AddInputFn addPitch = nullptr;
		AddInputFn addYaw = nullptr;
		bool  attempted = false;
		bool  ok = false;
		char  status[160] = "not resolved yet";
	};
	inline Resolved g_Calls;

	/* Escala efectiva de cada eje, medida en runtime (ver CalibrateFrom).
	 * InputYawScale/InputPitchScale estan deprecadas en UE5 y el motor decide
	 * por un flag si las aplica o usa 1.0, asi que el valor real no se puede
	 * dar por supuesto: se mide. */
	inline double g_YawScale = 1.0;
	inline double g_PitchScale = 1.0;
	inline bool   g_YawCalibrated = false;
	inline bool   g_PitchCalibrated = false;

	/* La direccion debe estar en una pagina ejecutable del propio modulo. */
	inline bool IsExecutable(uintptr_t addr, size_t size) {
		MEMORY_BASIC_INFORMATION mbi{};
		if (!VirtualQuery(reinterpret_cast<LPCVOID>(addr), &mbi, sizeof(mbi))) return false;
		if (mbi.State != MEM_COMMIT) return false;
		if (addr + size > reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize) return false;
		const DWORD exec = PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
		return (mbi.Protect & exec) != 0;
	}

	/* Verifica que en `fn` esta realmente una AddXInput y que ademas es la del
	 * eje esperado (el offset del FRotator va incrustado en la cola).
	 *
	 * Prologo (+0x00):
	 *     40 53                 push rbx
	 *     48 83 EC 30           sub  rsp, 30h
	 *     48 8B 01              mov  rax, [rcx]
	 *     48 8B D9              mov  rbx, rcx
	 * Cola (+0x73):
	 *     0F 5A C0              cvtps2pd xmm0, xmm0
	 *     F2 0F 58 83 <off32>   addsd  xmm0, [rbx+off]
	 *     F2 0F 11 83 <off32>   movsd  [rbx+off], xmm0
	 *     48 83 C4 30 5B C3     add rsp,30h ; pop rbx ; ret
	 */
	inline bool VerifySignature(uintptr_t fn, uint32_t wantOffset) {
		if (!IsValidPtr(fn) || !IsExecutable(fn, 0x8C)) return false;

		uint8_t b[0x8C] = {};
		if (!readBytes(fn, b, sizeof(b))) return false;

		static const uint8_t kProlog[12] = {
			0x40, 0x53, 0x48, 0x83, 0xEC, 0x30, 0x48, 0x8B, 0x01, 0x48, 0x8B, 0xD9
		};
		if (memcmp(b, kProlog, sizeof(kProlog)) != 0) return false;

		const uint8_t* t = b + 0x73;
		if (!(t[0] == 0x0F && t[1] == 0x5A && t[2] == 0xC0)) return false;
		if (!(t[3] == 0xF2 && t[4] == 0x0F && t[5] == 0x58 && t[6] == 0x83)) return false;
		uint32_t off1 = 0; memcpy(&off1, t + 7, 4);
		if (off1 != wantOffset) return false;
		if (!(t[11] == 0xF2 && t[12] == 0x0F && t[13] == 0x11 && t[14] == 0x83)) return false;
		uint32_t off2 = 0; memcpy(&off2, t + 15, 4);
		if (off2 != wantOffset) return false;
		if (!(t[19] == 0x48 && t[20] == 0x83 && t[21] == 0xC4 && t[22] == 0x30 &&
			  t[23] == 0x5B && t[24] == 0xC3)) return false;
		return true;
	}

	/* Resuelve una sola vez. Si falla, deja escrito el motivo para que se vea
	 * en la seccion Debug del menu en vez de fallar en silencio. */
	inline void Resolve() {
		if (g_Calls.attempted) return;
		g_Calls.attempted = true;

		const DWORD64 base = (DWORD64)GetModuleHandleA("Bodycam-Win64-Shipping.exe");
		if (!base) {
			strcpy_s(g_Calls.status, "game module not found (not injected?)");
			return;
		}

		const uintptr_t pitch = (uintptr_t)base + kRVA_AddPitchInput;
		const uintptr_t yaw = (uintptr_t)base + kRVA_AddYawInput;

		const bool okPitch = VerifySignature(pitch, offset::rotation_input_pitch);
		const bool okYaw = VerifySignature(yaw, offset::rotation_input_yaw);

		if (!okPitch || !okYaw) {
			sprintf_s(g_Calls.status,
			          "signature mismatch (pitch=%d yaw=%d) - game patched? using direct write",
			          (int)okPitch, (int)okYaw);
			return;
		}

		g_Calls.addPitch = reinterpret_cast<AddInputFn>(pitch);
		g_Calls.addYaw = reinterpret_cast<AddInputFn>(yaw);
		g_Calls.ok = true;
		strcpy_s(g_Calls.status, "AddPitchInput / AddYawInput verified");
	}

	/* La llamada en si, aislada y sin objetos C++ (requisito de __try). */
	__declspec(noinline) inline bool SafeInvoke(AddInputFn fn, void* pc, float v) {
		__try {
			fn(pc, v);
			return true;
		}
		__except (EXCEPTION_EXECUTE_HANDLER) {
			return false;
		}
	}

	/* Mide la escala real comparando lo que pedimos con lo que acabo entrando en
	 * RotationInput. Asi da igual que el motor aplique InputYawScale o 1.0, y se
	 * captura tambien el signo (InputPitchScale suele ser negativo).
	 *
	 * Se descarta la medida si el tick del juego consumio RotationInput entre
	 * las dos lecturas: se nota porque la razon sale fuera de rango o con el
	 * signo cambiado respecto a la medida que ya teniamos. */
	inline void CalibrateFrom(double before, double after, float sent,
	                          double& scale, bool& calibrated) {
		if (fabsf(sent) < 1e-5f) return;
		const double applied = after - before;
		if (fabs(applied) < 1e-9) return;

		const double measured = applied / (double)sent;
		if (!(measured == measured)) return;                 /* NaN */
		if (fabs(measured) < 0.01 || fabs(measured) > 100.0) return;

		if (!calibrated) { scale = measured; calibrated = true; return; }

		/* Ya hay una escala buena. Si el tick del juego consumio y reseteo
		 * RotationInput entre las dos lecturas, `applied` no es lo que aportamos
		 * nosotros y la medida es basura. Se detecta porque saldria con otro
		 * signo o desviada varias veces respecto a la que ya tenemos: la escala
		 * del motor es una constante, no cambia sola.
		 *
		 * Sin esta guarda una sola colision podia envenenar la escala y dejar el
		 * aimbot moviendose de mas o de menos. */
		if ((measured < 0.0) != (scale < 0.0)) return;       /* cambio de signo  */
		const double ratio = fabs(measured / scale);
		if (ratio < 0.34 || ratio > 3.0) return;             /* salto brusco     */

		scale = scale * 0.75 + measured * 0.25;              /* suavizado        */
	}

	/* Sondeo de calibracion.
	 *
	 * La primera vez no sabemos la escala real, y si aplicaramos el movimiento
	 * completo con una estimacion equivocada saldria un tiron. Se manda un valor
	 * minusculo (unas centesimas de grado, imperceptible), se mide lo que entro
	 * en RotationInput y con eso ya se aplica el movimiento real en este mismo
	 * frame, sin retraso.
	 *
	 * Si IsLookInputIgnored() esta activo (menu, pausa) no entra nada, la medida
	 * se descarta y el aimbot simplemente no mueve: que es lo correcto. */
	inline void ProbeScale(uintptr_t pc, bool yaw) {
		if (!g_Calls.ok || !IsValidPtr(pc)) return;
		AddInputFn fn = yaw ? g_Calls.addYaw : g_Calls.addPitch;
		if (!fn) return;
		const int off = yaw ? offset::rotation_input_yaw : offset::rotation_input_pitch;
		double& scale = yaw ? g_YawScale : g_PitchScale;
		bool& done = yaw ? g_YawCalibrated : g_PitchCalibrated;

		const float probe = 0.05f;
		double before = 0.0, after = 0.0;
		/* Si cualquiera de las dos lecturas falla, read<> deja un 0 y la resta
		 * daria una medida inventada. Mejor no calibrar este frame. */
		if (!read<double>(pc + off, before)) return;
		if (!SafeInvoke(fn, reinterpret_cast<void*>(pc), probe)) return;
		if (!read<double>(pc + off, after)) return;
		CalibrateFrom(before, after, probe, scale, done);
	}

	/* Mueve la mira `degYaw` / `degPitch` GRADOS usando las funciones del juego.
	 * Devuelve false si no se pudo (no resuelto, PC invalido o la llamada fallo),
	 * para que el llamante caiga al metodo alternativo. */
	inline bool AddLookInput(uintptr_t pc, double degYaw, double degPitch, double maxStepDeg) {
		if (!g_Calls.ok || !IsValidPtr(pc)) return false;

		/* Calibrar antes del primer movimiento real. */
		if (!g_YawCalibrated && fabs(degYaw) > 1e-4) ProbeScale(pc, true);
		if (!g_PitchCalibrated && fabs(degPitch) > 1e-4) ProbeScale(pc, false);

		/* Tope duro: si el calculo se fuera de madre, esto evita un giro absurdo. */
		if (maxStepDeg < 0.1) maxStepDeg = 0.1;
		if (degYaw > maxStepDeg) degYaw = maxStepDeg;
		if (degYaw < -maxStepDeg) degYaw = -maxStepDeg;
		if (degPitch > maxStepDeg) degPitch = maxStepDeg;
		if (degPitch < -maxStepDeg) degPitch = -maxStepDeg;

		void* pcPtr = reinterpret_cast<void*>(pc);
		bool any = false;

		/* La funcion multiplica por la escala del motor, asi que para mover N
		 * grados hay que pasarle N/escala. Se acota el valor final ademas del
		 * angulo: si la escala medida fuera anormalmente pequena, la division
		 * podria disparar el valor. Con |v| <= 4000 y el minimo de escala que
		 * CalibrateFrom acepta (0.01), el giro real nunca pasa de ~40 grados. */
		auto clampInput = [](double x) -> float {
			if (x > 4000.0) x = 4000.0;
			if (x < -4000.0) x = -4000.0;
			return (float)x;
		};

		if (fabs(degYaw) > 1e-4) {
			const double sc = (fabs(g_YawScale) > 1e-6) ? g_YawScale : 1.0;
			const float  v = clampInput(degYaw / sc);
			double before = 0.0, after = 0.0;
			const bool okBefore = read<double>(pc + offset::rotation_input_yaw, before);
			if (!SafeInvoke(g_Calls.addYaw, pcPtr, v)) return false;
			const bool okAfter = read<double>(pc + offset::rotation_input_yaw, after);
			/* Solo se calibra si ambas lecturas valieron: con una fallida, la
			 * resta seria contra un 0 inventado. El movimiento ya se aplico. */
			if (okBefore && okAfter)
				CalibrateFrom(before, after, v, g_YawScale, g_YawCalibrated);
			any = true;
		}

		if (fabs(degPitch) > 1e-4) {
			const double sc = (fabs(g_PitchScale) > 1e-6) ? g_PitchScale : 1.0;
			const float  v = clampInput(degPitch / sc);
			double before = 0.0, after = 0.0;
			const bool okBefore = read<double>(pc + offset::rotation_input_pitch, before);
			if (!SafeInvoke(g_Calls.addPitch, pcPtr, v)) return false;
			const bool okAfter = read<double>(pc + offset::rotation_input_pitch, after);
			if (okBefore && okAfter)
				CalibrateFrom(before, after, v, g_PitchScale, g_PitchCalibrated);
			any = true;
		}

		return any;
	}

	/* Alternativa sin llamar a nada: sumar directamente a RotationInput.
	 * Hace lo mismo que las funciones salvo aplicar la escala y respetar
	 * IsLookInputIgnored(). Es el respaldo si la firma no valida. */
	inline bool AddLookInputDirect(uintptr_t pc, double degYaw, double degPitch, double maxStepDeg) {
		if (!IsValidPtr(pc)) return false;
		if (maxStepDeg < 0.1) maxStepDeg = 0.1;
		if (degYaw > maxStepDeg) degYaw = maxStepDeg;
		if (degYaw < -maxStepDeg) degYaw = -maxStepDeg;
		if (degPitch > maxStepDeg) degPitch = maxStepDeg;
		if (degPitch < -maxStepDeg) degPitch = -maxStepDeg;

		bool ok = false;
		if (fabs(degYaw) > 1e-4) {
			double cur = 0.0;
			if (read<double>(pc + offset::rotation_input_yaw, cur))
				ok |= write<double>(pc + offset::rotation_input_yaw, cur + degYaw);
		}
		if (fabs(degPitch) > 1e-4) {
			double cur = 0.0;
			if (read<double>(pc + offset::rotation_input_pitch, cur))
				ok |= write<double>(pc + offset::rotation_input_pitch, cur + degPitch);
		}
		return ok;
	}

} // namespace GameCalls
