#include <stdio.h>
#include <stdint.h>
//#include <malloc.h>
#include <stdarg.h>

#include "engine.h"

int verbose = 4;
#define LOG(v,...) {if ( (v) <= verbose ) printf(__VA_ARGS__);}
#define CLR_NORMAL  "\033[0m"
#define CLR_RED     "\033[31m"
#define CLR_GREEN   "\033[32m"

resource_arc_t *res01, *res02;

static int func_nargs = 0;

#define SCR_STACK_SIZE  64
static int32_t scr_stack[SCR_STACK_SIZE];
static unsigned scr_stack_pointer;

static void *call_stack[32][2];
static int call_stack_pointer = 0;

static void stack_push( int data )
{
    scr_stack[ scr_stack_pointer ] = data;

    scr_stack_pointer = (scr_stack_pointer + 1) % SCR_STACK_SIZE;

    if ( scr_stack_pointer == 0 )
    {
        fprintf( stderr, "Warning: stack overflow\n" );
        exit(1);
    }
}

static int stack_pop()
{
    scr_stack_pointer = (scr_stack_pointer - 1) % SCR_STACK_SIZE;

    if ( scr_stack_pointer == SCR_STACK_SIZE )
    {
        fprintf( stderr, "Warning: stack overflow\n" );
        exit(1);
    }

    return scr_stack[ scr_stack_pointer ];
}

#define SET_ARG(arg) (sizeof(arg)),(&(arg))
#define NULL_ARG (0),(NULL)

static int func_get_args( int arg_size, ... )
{
    va_list ap;
    void *arg;
    unsigned sp = (scr_stack_pointer - func_nargs) % SCR_STACK_SIZE;
    scr_stack_pointer = sp;

    va_start( ap, arg_size );

    while ( arg_size >= 0 )
    {
        arg = va_arg( ap, void * );
        if ( arg )
        {
#if 0
            switch ( arg_size )
            {
                case 0:
                    *((void **)arg) = scr_stack[ sp ];
                    break;
                case 1:
                    *((int8_t *)arg) = scr_stack[ sp ];
                    break;
                case 2:
                    *((int16_t *)arg) = scr_stack[ sp ];
                    break;
                case 4:
                    *((int32_t *)arg) = scr_stack[ sp ];
                    break;
                case 8:
                    *((int64_t *)arg) = scr_stack[ sp ];
                    break;
                default:
                    printf("!!!! unknown argument size %d !!!!!\n", arg_size );
            }
#else
            if ( arg_size <= sizeof(*scr_stack) )
                memcpy( arg, &scr_stack[sp], arg_size );
#endif
        }

        arg_size = va_arg( ap, int );

        sp = ( sp + 1 ) % SCR_STACK_SIZE;
    }
    func_nargs = 0;

    va_end( ap );
}

static void *script_data;
static char base[1024];
static int32_t base_off = 0;
static int32_t vars[0x400] = { 0 };

static struct
{
    const char *group;
    const char *command;
    void *(*func)( void *data, void *pos );
} ops[0x400];

#define SET_OP(i,c,f) { ops[i].command = c; ops[i].func = f; }
static const char op_groups[][5] = { "ctrl", "sys ", "msg ", "slct", "snd ", "grp " };
static const char null_str[] = "";

#include "script_op.c"

void prepare_ops()
{
    int i;

    for ( i = 0x000; i < 0x100; i ++ )
        ops[i].group = op_groups[0];
    for ( i = 0x100; i < 0x140; i ++ )
        ops[i].group = op_groups[1];
    for ( i = 0x140; i < 0x160; i ++ )
        ops[i].group = op_groups[2];
    for ( i = 0x160; i < 0x180; i ++ )
        ops[i].group = op_groups[3];
    for ( i = 0x180; i < 0x200; i ++ )
        ops[i].group = op_groups[4];
    for ( i = 0x200; i < 0x400; i ++ )
        ops[i].group = op_groups[5];

    for ( i = 0x000; i < 0x400; i ++ )
        ops[i].func = op_pop_noarg;

    SET_OP(0x000, "push_dword", op_push_dw)
    SET_OP(0x001, "push_offset", op_push_off)
    SET_OP(0x002, "push_base_offset", op_push_base_off)
    SET_OP(0x003, "push_string", op_push_str)
    SET_OP(0x008, "load", op_load)
    SET_OP(0x009, "move", op_move)
    SET_OP(0x00A, "move_arg", op_move_arg)
    SET_OP(0x010, "load_base", op_load_base)
    SET_OP(0x011, "store_base", op_store_base)
    SET_OP(0x018, "jmp", op_jmp)
    SET_OP(0x019, "jnz", op_jnz)
    SET_OP(0x01E, "reg_exception_handler", op_pop_noarg)
    SET_OP(0x01F, "unreg_exception_handler", NULL)
    SET_OP(0x01B, "ret", op_ret)
    SET_OP(0x020, "add", op_add)
    SET_OP(0x021, "sub", op_sub)
    SET_OP(0x022, "mul", op_mul)
    SET_OP(0x030, "eq", op_eq)
    SET_OP(0x031, "eq", op_neq)
    SET_OP(0x032, "leq", op_leq)
    SET_OP(0x033, "geq", op_geq)
    SET_OP(0x034, "lt", op_lt)
    SET_OP(0x035, "gt", op_gt)
    SET_OP(0x038, "flor", op_flag_or)
    SET_OP(0x03F, "nargs", op_nargs)
    SET_OP(0x07F, "line", op_line)
    SET_OP(0x0E0, "save_var", op_save_var)
    SET_OP(0x0E1, "load_var", op_load_var)
    SET_OP(0x0E3, NULL, NULL)
    SET_OP(0x0F0, "call", op_call)
    SET_OP(0x0F4, "exit", op_exit)

    // sys
    SET_OP(0x110, "delay", op_sys_delay)
    SET_OP(0x119, NULL, NULL)

    // msg
    SET_OP(0x140, "put", op_msg_put)
    SET_OP(0x141, "clear", op_msg_clear)

    SET_OP(0x151, NULL, NULL)
    SET_OP(0x156, NULL, NULL)
    SET_OP(0x160, NULL, NULL)

    // select
    SET_OP(0x170, "show_select", op_slct_show_select)

    SET_OP(0x180, "set_bgm", op_pop_noarg)
    SET_OP(0x1A0, "play_sfx", op_pop_noarg)
    SET_OP(0x1B2, "play_voice", op_pop_noarg)
    SET_OP(0x1B4, "set_scene", op_pop_noarg)

    SET_OP(0x260, "set_background", op_grp_set_bkgr)
    SET_OP(0x261, "set_background_se", op_grp_set_bkgr)    // with effect actully
    SET_OP(0x268, "hide_background", op_grp_hide_bkgr)

    SET_OP(0x280, "char_set", op_grp_char_set) // put? character on screen
    SET_OP(0x288, "char_hide", op_grp_char_hide) // hide? character from screen
}

void interpret()
{
    void *pos = script_data;

    func_nargs = 0;
    while ( pos )
    {
        int32_t opcode = *((int32_t *)pos);
        LOG( 3, "%.8X %.2d ", pos - script_data, scr_stack_pointer );
        pos += 4;

        if ( opcode > 0x400 )
        {
            LOG( 0, "Bad opcode (%x)! Returning!\n", opcode );
            return;
        }

        if ( ops[opcode].command )
            LOG( 3, CLR_GREEN"%s->%s ", ops[opcode].group, ops[opcode].command )
        else
            LOG( 3, CLR_RED"%s->%.3x ", ops[opcode].group, opcode )
        LOG( 0, CLR_NORMAL );

        if ( ops[opcode].func )
            pos = ops[opcode].func( script_data, pos );   // command should output some info to stdout
        else
        {
            LOG( 4, "unknown, ignoring (popping %d args)\n", func_nargs );
            while ( func_nargs -- )
                stack_pop();
            func_nargs = 0;
        }

        //getchar();
    }
}

int script_thread(void *unused)
{
    void *script0;

    res01 = open_resource( BGI_ROOT"data01000.arc" );
    res02 = open_resource( BGI_ROOT"data02000.arc" );

    script0 = get_resource( res01, "main" );
    //script0 = get_resource( res01, "amain" );
    //script0 = get_resource( res01, "a490" );
    //close_resource( res01 );
    script_data = script0;
    interpret();

    return(0);
}
