# Anim Assistant

#### Description
##### 1.Print the Virtual Bone Datas of the Skeleton to the Log `PrintVirtualBone`
###### Params:
###### 1.a Target Skeleton
##### 2.Retarget the Animation Blueprint `RetargetAnim`
###### Params:
###### 2.a Source Skeleton
###### 2.b Target Skeleton
###### 2.c Source Animation Blueprint
###### 2.d Target Animation Folder FE \Game\AdvancedLocomotionV4\CharacterAssets
##### 3.Create the Virtual Bones `CreateVirtualBone`
###### Params:
###### 3.a Target Skeleton
###### 3.b Json Script Format Bone List:（Copy and paste the following text into the JSONBoneList parameter）
```
{
    "BoneList":
    [
        {
            "BoneName":"VB Curves",
            "SourceBone":"root",
            "TargetBone":"root"
        },
        {
            "BoneName":"VB RHS_ik_hand_gun",
            "SourceBone":"Hand_R",
            "TargetBone":"ik_hand_gun"
        },
        {
            "BoneName":"VB LHS_ik_hand_gun",
            "SourceBone":"Hand_L",
            "TargetBone":"ik_hand_gun"
        },
        {
            "BoneName":"VB RHS_ik_hand_l",
            "SourceBone":"VB RHS_ik_hand_gun",
            "TargetBone":"Hand_L"
        },
        {
            "BoneName":"VB LHS_ik_hand_r",
            "SourceBone":"VB LHS_ik_hand_gun",
            "TargetBone":" Hand_R"
        },
        {
            "BoneName":"VB ik_foot_l_Offset",
            "SourceBone":"ik_foot_l",
            "TargetBone":"ik_foot_l"
        },
        {
            "BoneName":"VB ik_foot_r_Offset",
            "SourceBone":"ik_foot_r",
            "TargetBone":"ik_foot_r"
        },
        {
            "BoneName":"VB ik_knee_target_l",
            "SourceBone":"ik_foot_l",
            "TargetBone":"calf_l"
        },
        {
            "BoneName":"VB ik_knee_target_r",
            "SourceBone":"ik_foot_r",
            "TargetBone":"calf_r"
        },
        {
            "BoneName":"VB foot_target_l",
            "SourceBone":"ik_foot_root",
            "TargetBone":"ik_foot_l"
        },
        {
            "BoneName":"VB foot_target_r",
            "SourceBone":"ik_foot_root",
            "TargetBone":"ik_foot_r"
        }
    ]
}
```