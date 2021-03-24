# 动画助手

#### 介绍
##### 1.将虚拟骨骼数据打印到日志中 `PrintVirtualBone`
###### 参数：
###### 1.a 目标骨骼
##### 2.重定位动画蓝图 `RetargetAnim`
###### 参数：
###### 2.a 源骨骼
###### 2.b 目标骨骼
###### 2.c 源动画蓝图
###### 2.d 目标动画文件夹 例如\Game\AdvancedLocomotionV4\CharacterAssets
##### 3.创建虚拟骨骼 `CreateVirtualBone`
###### 参数：
###### 3.a 目标骨骼
###### 3.b Json脚本格式骨骼列表:（将下面文本复制粘贴到JsonBoneList参数中）
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
