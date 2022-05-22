#ifndef __QUAKEDEF_H
#define __QUAKEDEF_H

// These definitions are all copied from QuakeSpasm

#define U_MOREBITS      (1<<0)
#define U_ORIGIN1       (1<<1)
#define U_ORIGIN2       (1<<2)
#define U_ORIGIN3       (1<<3)
#define U_ANGLE2        (1<<4)
#define U_STEP          (1<<5)
#define U_FRAME         (1<<6)
#define U_SIGNAL        (1<<7)
#define U_ANGLE1        (1<<8)
#define U_ANGLE3        (1<<9)
#define U_MODEL         (1<<10)
#define U_COLORMAP      (1<<11)
#define U_SKIN          (1<<12)
#define U_EFFECTS       (1<<13)
#define U_LONGENTITY    (1<<14)
#define U_EXTEND1       (1<<15)
#define U_ALPHA         (1<<16)
#define U_FRAME2        (1<<17)
#define U_MODEL2        (1<<18)
#define U_LERPFINISH    (1<<19)
#define U_SCALE         (1<<20)
#define U_UNUSED21      (1<<21)
#define U_UNUSED22      (1<<22)
#define U_EXTEND2       (1<<23)


#define svc_bad                 0
#define svc_nop                 1
#define svc_disconnect          2
#define svc_updatestat          3
#define svc_version             4
#define svc_setview             5
#define svc_sound               6
#define svc_time                7
#define svc_print               8
#define svc_stufftext           9
#define svc_setangle            10
#define svc_serverinfo          11
#define svc_lightstyle          12
#define svc_updatename          13
#define svc_updatefrags         14
#define svc_clientdata          15
#define svc_stopsound           16
#define svc_updatecolors        17
#define svc_particle            18
#define svc_damage              19
#define svc_spawnstatic         20
#define svc_spawnbaseline       22
#define svc_temp_entity         23
#define svc_setpause            24
#define svc_signonnum           25
#define svc_centerprint         26
#define svc_killedmonster       27
#define svc_foundsecret         28
#define svc_spawnstaticsound    29
#define svc_intermission        30
#define svc_finale              31
#define svc_cdtrack             32
#define svc_sellscreen          33
#define svc_cutscene            34
#define svc_skybox              37
#define svc_bf                  40
#define svc_fog                 41
#define svc_spawnbaseline2      42
#define svc_spawnstatic2        43
#define svc_spawnstaticsound2   44
#define svc_botchat             38
#define svc_setviews            45
#define svc_updateping          46
#define svc_updatesocial        47
#define svc_updateplinfo        48
#define svc_rawprint            49
#define svc_servervars          50
#define svc_seq                 51
#define svc_achievement         52
#define svc_chat                53
#define svc_levelcompleted      54
#define svc_backtolobby         55
#define svc_localsound          56



#endif /* __QUAKEDEF_H */
