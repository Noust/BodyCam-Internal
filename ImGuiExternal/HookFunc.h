#pragma once
#include "include.h"

/* ---------------------------------------------------------------------------
 *  Offsets de Bodycam-Win64-Shipping.exe
 *
 *  TODOS derivados de ESTE binario. Ver context/REVERSE_ENGINEERING.md para la
 *  evidencia de cada uno (reflexion UECodeGen / desensamblado / sizeof).
 *
 *  Regla: si tocas un offset, actualiza tambien el .md. Y comprueba que cabe
 *  dentro del sizeof de su clase (esa comprobacion fue la que delato que el
 *  viejo player_array = 0x338 era imposible: AGameStateBase mide 0x300).
 * ------------------------------------------------------------------------- */

enum offset {
	/* ---- UWorld (sizeof 0x908) ---- */
	persistent_level        = 0x30,
	game_state              = 0x160,
	game_instance           = 0x1D8,

	/* ---- UGameInstance (0x1C0) / ULocalPlayer (0x2B0) ---- */
	local_player            = 0x38,    /* UGameInstance::LocalPlayers (TArray)      */
	player_controller       = 0x30,    /* ULocalPlayer::PlayerController            */
	aspect_axis_constraint  = 0xB8,    /* ULocalPlayer::AspectRatioAxisConstraint   */

	/* ---- APlayerController (0x858) ---- */
	acknowledged_pawn       = 0x350,
	camera_manager          = 0x360,

	/* ---- APlayerCameraManager (0x25A0) ---- */
	default_fov             = 0x2C0,
	camera_cache            = 0x1410,  /* FCameraCacheEntry { float TimeStamp; POV @0x10 } */
	pov_info                = 0x1420,  /* = camera_cache + 0x10                     */
	camera_cache_last       = 0x1C50,
	pov_info_last           = 0x1C60,

	/* ---- FMinimalViewInfo (0x830), relativos al POV ---- */
	mvi_location            = 0x00,
	mvi_rotation            = 0x18,
	mvi_fov                 = 0x30,
	mvi_aspect_ratio        = 0x5C,
	mvi_flags               = 0x68,    /* bit 0 = bConstrainAspectRatio             */

	/* ---- AGameStateBase (0x300) ---- */
	player_array            = 0x2C0,   /* TArray<APlayerState*>                     */
	player_array_data       = 0x00,    /* Data esta en +0 del TArray                */
	player_array_num        = 0x08,
	player_array_stride     = 0x08,    /* punteros, no structs inline               */

	/* ---- APlayerState (0x360) ---- */
	ps_pawn                 = 0x320,   /* PawnPrivate                               */
	ps_name                 = 0x340,   /* PlayerNamePrivate (FString)               */

	/* ---- ABodycamPlayerState (0x438) ---- */
	ps_team_id              = 0x388,   /* int32, replicado (CPF_Net)                */
	ps_kills                = 0x38C,
	ps_deaths               = 0x390,

	/* ---- AActor (0x2A8) ---- */
	root_component          = 0x1B8,

	/* ---- USceneComponent (0x2B8) ---- */
	relative_location       = 0x128,
	relative_rotation       = 0x140,
	velocity                = 0x170,
	component_to_world      = 0x1D0,   /* FTransform (0x60). No es UPROPERTY        */
	c2w_translation         = 0x1F0,   /* = component_to_world + 0x20               */
	c2w_scale               = 0x210,   /* = component_to_world + 0x40               */

	/* ---- APawn (0x328) / AController (0x340) ---- */
	remote_view_pitch       = 0x2BA,   /* uint8, replicado                          */
	player_state            = 0x2C8,   /* APawn::PlayerState                        */
	pawn_controller         = 0x2D8,
	control_rotation        = 0x320,   /* AController::ControlRotation (3 doubles)  */

	/* ---- APlayerController (0x858) — sistema de input de rotacion ---------
	 * RotationInput NO es UPROPERTY. Localizado por ASM en AddPitchInput /
	 * AddYawInput / AddRollInput, que hacen  RotationInput.X += val * Scale.
	 * Que las tres escalas reflexionadas (0x540/0x544/0x548) queden justo
	 * detras del FRotator de 24 bytes (0x528..0x540) confirma el layout.
	 *
	 * Esta es la via CORRECTA para mover la mira: el juego consume
	 * RotationInput en su propio UpdateRotation, dentro del game thread.
	 * Escribir ControlRotation directamente pelea con el juego, que lo
	 * recalcula cada tick a partir de este mismo RotationInput. */
	rotation_input          = 0x528,   /* FRotator (3 doubles)                      */
	rotation_input_pitch    = 0x528,
	rotation_input_yaw      = 0x530,
	rotation_input_roll     = 0x538,
	input_yaw_scale         = 0x540,
	input_pitch_scale       = 0x544,
	input_roll_scale        = 0x548,
	pc_player_input         = 0x420,
	pc_target_view_rotation = 0x378,

	/* ---- ACharacter (0x650) ---- */
	skeletal_mesh_component = 0x328,   /* ACharacter::Mesh                          */
	character_movement      = 0x330,
	capsule_component       = 0x338,

	/* ---- UCapsuleComponent (0x510) ---- */
	capsule_half_height     = 0x508,   /* adyacentes: 1 sola lectura de 8 bytes     */
	capsule_radius          = 0x50C,

	/* ---- USkinnedMeshComponent (0x890) ---- */
	skeletal_mesh_asset_old = 0x520,   /* SkeletalMesh (deprecada)                  */
	skinned_asset           = 0x528,
	leader_pose_component   = 0x530,
	active_bone_array       = 0x598,   /* ComponentSpaceTransformsArray[0].Data     */
	bone_array_stride       = 0x10,    /* sizeof(TArray) -> [i] = 0x598 + 0x10*i    */
	bone_edit_index         = 0x5DC,   /* CurrentEditableComponentTransforms        */
	bone_buffer_index       = 0x5E0,   /* CurrentReadComponentTransforms (0 o 1)    */
	leader_bone_map         = 0x608,
	bone_stride             = 0x60,    /* sizeof(FTransform), confirmado por ASM    */

	/* ---- FTransform (0x60) ---- */
	ft_rotation             = 0x00,    /* FQuat, 4 doubles                          */
	ft_translation          = 0x20,    /* FVector, 3 doubles                        */
	ft_scale                = 0x40,

	/* ---- ABodycamCharacter (0x680) ---- */
	bc_ability_system       = 0x658,
	bc_pawn_ext             = 0x660,
	bc_character_set        = 0x668,   /* UCharacterAttributeSet*                   */

	/* ---- UCharacterAttributeSet (0xD8) ------------------------------------
	 * Cada atributo es un FGameplayAttributeData de 16 bytes:
	 *     vtable @0x00 | float BaseValue @0x08 | float CurrentValue @0x0C
	 * Usar SIEMPRE CurrentValue: BaseValue no lleva los modificadores de GAS.
	 * -------------------------------------------------------------------- */
	attr_health             = 0x88,
	attr_max_health         = 0x98,
	attr_gadget_cooldown    = 0xA8,
	attr_healing            = 0xB8,
	attr_damage             = 0xC8,
	attr_current_value      = 0x0C,    /* sumar a cualquiera de los de arriba       */
	attr_base_value         = 0x08,

	/* atajos ya sumados, que es como se usan en la practica */
	health_current          = 0x94,    /* attr_health     + attr_current_value      */
	max_health_current      = 0xA4,    /* attr_max_health + attr_current_value      */

	/* ---- FReferenceSkeleton (dentro del USkinnedAsset) --------------------
	 * El offset del struct dentro del asset NO es fijo ni esta reflexionado:
	 * se localiza en runtime escaneando y validando invariantes (ver
	 * WorldToScreen.hpp). Estos son los offsets DENTRO del struct.
	 * -------------------------------------------------------------------- */
	refskel_raw_bone_info   = 0x00,    /* TArray<FMeshBoneInfo>                     */
	refskel_raw_bone_pose   = 0x10,    /* TArray<FTransform>                        */
	refskel_final_bone_info = 0x20,    /* TArray<FMeshBoneInfo>                     */
	refskel_final_bone_pose = 0x30,
	mesh_bone_info_stride   = 0x0C,    /* FName Name @0x00 ; int32 ParentIndex @0x08 */
	mesh_bone_info_parent   = 0x08,
};

/* Enum de ULocalPlayer::AspectRatioAxisConstraint (EAspectRatioAxisConstraint) */
enum EAspectAxis : int {
	AspectAxis_MaintainYFOV   = 0,
	AspectAxis_MaintainXFOV   = 1,
	AspectAxis_MajorAxisFOV   = 2,
};

/* Limites de cordura. Cualquier recorrido debe acotarse: un offset malo tiene
 * que producir datos raros, nunca un crash ni un bucle infinito. */
namespace limits {
	inline constexpr int   kMaxPlayers   = 128;
	inline constexpr int   kMaxBones     = 1024;
	inline constexpr int   kMaxNameLen   = 64;
	inline constexpr int   kMaxParents   = 256;
	inline constexpr float kMinHealth    = 0.0f;
	inline constexpr float kMaxHealth    = 100000.0f;
}

class hookclass {
public:
	bool Detour32(void* src, void* dst, int len);
	void Patch(BYTE* addr, BYTE* bytes, unsigned int size);
	bool Hook(BYTE* pTarget, BYTE* pHook, UINT Length);
	MODULEINFO GetModuleInfo(char* szModule);
	DWORD64 FindPattern(char* module, char* pattern, char* mask);
	void GetAddreses();
};

inline hookclass* hooks = nullptr;
