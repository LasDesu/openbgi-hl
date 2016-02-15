#include <unistd.h>

void *op_push_dw( void *data, void *pos )
{
    int32_t dw = *((int32_t *)pos);

    stack_push( dw );
    LOG( 3, "pushing %.8X\n", dw );

    return (pos + 4);
}

void *op_push_off( void *data, void *pos )
{
    uint32_t off = *((uint32_t *)pos);

    stack_push( off );
    LOG( 3, "pushing %.8X\n", off );

    return (pos + 4);
}

void *op_push_base_off( void *data, void *pos )
{
    uint32_t off = base_off + *((uint32_t *)pos);

    stack_push( off );
    LOG( 3, "push %.8X\n", off );

    return (pos + 4);
}

void *op_push_str( void *data, void *pos )
{
    uint32_t off = *((uint32_t *)pos);

    stack_push( off );
    LOG( 3, "pushing \"%s\"\n", (char *)data + off );

    return (pos + 4);
}

void *op_load( void *data, void *pos )
{
    uint32_t size = *((uint32_t *)pos);
    uint32_t addr, d;

    addr = stack_pop();
    //d = base[ addr ];
    memcpy( &d, base + addr, 1 << size );
    LOG( 3, "load %.8X from %.8X\n", d, addr );
    stack_push( d );

    return (pos + 4);
}

void *op_move( void *data, void *pos )
{
    uint32_t size = *((uint32_t *)pos);
    uint32_t d, addr;

    d = stack_pop();
    addr = stack_pop();

    LOG( 3, "move %.8X to %.8X\n", d, addr );
    memcpy( base + addr, &d, 1 << size );

    return (pos + 4);
}

void *op_move_arg( void *data, void *pos )
{
    uint32_t size = *((uint32_t *)pos);
    uint32_t d, addr;

    addr = stack_pop();
    d = stack_pop();

    LOG( 3, "move %.8X to %.8X\n", d, addr );
    memcpy( base + addr, &d, 1 << size );

    return (pos + 4);
}

void *op_load_base( void *data, void *pos )
{
    stack_push( base_off );

    LOG( 3, "load base %.8X\n", base_off );

    return pos;
}

void *op_store_base( void *data, void *pos )
{
    base_off = stack_pop();

    LOG( 3, "store base %.8X\n", base_off );
    if ( base_off < 0 )
        exit(1);

    return pos;
}

void *op_jmp( void *data, void *pos )
{
    uint32_t off;

    off = stack_pop();
    LOG( 2, "jmp to %x\n", off );

    return (data + off);
}

void *op_jnz( void *data, void *pos )
{
    uint32_t addr = *((uint32_t *)pos);
    uint32_t off, val;

    off = stack_pop();
    val = stack_pop();
    LOG( 2
        , "%sjnz to %x\n", val ? "no " : "", off );
    if ( !val )
        return (data + off);

    return (pos + 4);
}

void *op_pop_noarg( void *data, void *pos )
{
    while ( func_nargs-- > 1 )
        stack_pop();
    func_nargs = 0;
    stack_pop();

    LOG( 3, "just popping stack\n" );

    return pos;
}

void *op_unknown( void *data, void *pos )
{
    LOG( 3, "unknown opcode\n" );
    exit(1);

    return pos;
}

void *op_ret( void *data, void *pos )
{
    LOG( 1, "return\n");

    if ( !call_stack_pointer )
        return 0;

    free( data );

    script_data = call_stack[ --call_stack_pointer ][0];
    return( call_stack[ call_stack_pointer ][1] );
}

void *op_add( void *data, void *pos )
{
    int32_t op1, op2, res;

    op2 = stack_pop();
    op1 = stack_pop();

    res = op1 + op2;
    LOG( 3, "%d + %d = %d\n", op1, op2, res);
    stack_push( res );

    return pos;
}

void *op_sub( void *data, void *pos )
{
    int32_t op1, op2, res;

    op2 = stack_pop();
    op1 = stack_pop();

    res = op1 - op2;
    LOG( 3, "%d - %d = %d\n", op1, op2, res);
    stack_push( res );

    return pos;
}

void *op_mul( void *data, void *pos )
{
    int32_t op1, op2, res;

    op2 = stack_pop();
    op1 = stack_pop();

    res = op1 * op2;
    LOG( 3, "%d * %d = %d\n", op1, op2, res);
    stack_push( res );

    return pos;
}

void *op_eq( void *data, void *pos )
{
    int32_t op1, op2, res;

    op1 = stack_pop();
    op2 = stack_pop();

    res = ( op1 == op2 );

    LOG( 2, "%d %s %d\n", op1, res ? "==" : "<>", op2);
    stack_push( res );

    return pos;
}

void *op_neq( void *data, void *pos )
{
    int32_t op1, op2, res;

    op2 = stack_pop();
    op1 = stack_pop();

    res = ( op1 != op2 );

    LOG( 2, "%d %s %d\n", op1, res ? "<>" : "==", op2);
    stack_push( res );

    return pos;
}

void *op_leq( void *data, void *pos )
{
    int32_t op1, op2, res;

    op2 = stack_pop();
    op1 = stack_pop();

    res = ( op1 <= op2 );

    LOG( 2, "%d %s %d\n", op1, res ? "<=" : ">", op2);
    stack_push( res );

    return pos;
}

void *op_geq( void *data, void *pos )
{
    int32_t op1, op2, res;

    op2 = stack_pop();
    op1 = stack_pop();

    res = ( op1 >= op2 );

    LOG( 2, "%d %s %d\n", op1, res ? ">=" : "<", op2);
    stack_push( res );

    return pos;
}

void *op_lt( void *data, void *pos )
{
    int32_t op1, op2, res;

    op2 = stack_pop();
    op1 = stack_pop();

    res = ( op1 < op2 );

    LOG( 2, "%d %s %d\n", op1, res ? "<" : ">=", op2);
    stack_push( res );

    return pos;
}

void *op_gt( void *data, void *pos )
{
    int32_t op1, op2, res;

    op2 = stack_pop();
    op1 = stack_pop();

    res = ( op1 > op2 );

    LOG( 2, "%d %s %d\n", op1, res ? ">" : "<=", op2);
    stack_push( res );

    return pos;
}

void *op_flag_or( void *data, void *pos )
{
    int32_t op1, op2, res;

    op1 = stack_pop();
    op2 = stack_pop();

    res = op1 + op2;

    LOG( 3, "%d + %d = %d\n", op1, op2, res);
    stack_push( res );

    return pos;
}

void *op_nargs( void *data, void *pos )
{
    int32_t n = *((int32_t *)pos);

    func_nargs = n;
    LOG( 3, "%d args\n", n );

    return (pos + 4);
}

void *op_line( void *data, void *pos )
{
    uint32_t off = *((uint32_t *)pos);
    int num = *((uint32_t *)(pos + 4));

    LOG( 3, "#%d \"%s\"\n", num, (char *)data + off );

    return (pos + 8);
}

void *op_save_var( void *data, void *pos )
{
    int32_t n, val;

    val = stack_pop();
    n = stack_pop();
    LOG( 2, "save %x -> var %x\n", val, n );
    vars[n] = val;

    return pos;
}

void *op_load_var( void *data, void *pos )
{
    int32_t n, val;

    n = stack_pop();
    val = vars[n];
    LOG( 2, "load %x <- var %x\n", val, n );
    stack_push( val );

    return pos;
}

void *op_call( void *data, void *pos )
{
    int32_t off;
    void *script;

    off = stack_pop();
    LOG( 1, "call %s\n", (char *)data + off );

    script = get_resource( res01, (char *)data + off );
    call_stack[ call_stack_pointer ][0] = script_data;
    call_stack[ call_stack_pointer++ ][1] = pos;

    script_data = script;
    func_nargs = 0;

    return script;
}

void *op_exit( void *data, void *pos )
{
    LOG( 1, "exit\n");

    while ( call_stack_pointer-- )
    {
        free( data );
        data = call_stack[ call_stack_pointer ][0];
    }

    return( 0 );
}

// sys block
void *op_sys_delay( void *data, void *pos )
{
    int time = stack_pop();

    LOG( 3, "delay %d ms\n", time );
    //usleep( time * 1000 );

    return pos;
}

// msg block

void *op_msg_put( void *data, void *pos )
{
    int32_t str;

    func_get_args( SET_ARG(str), -1 );

    LOG( 3, "\"%s\"\n", (char *)data + str );
    display_put_wait( (char *)data + str );

    return pos;
}

void *op_msg_clear( void *data, void *pos )
{
    display_clear();

    LOG( 3, "\n" );

    return pos;
}

// select
void *op_slct_show_select( void *data, void *pos )
{
    int32_t num, off;
    int32_t *opts;
    int i;

    func_get_args( SET_ARG(num), SET_ARG(off), -1 );
    opts = base + off;

    LOG( 3, "select from %d options\n", num );
    vars[0] = show_select( data, opts, num );

    return pos;
}

// grp

void *op_grp_set_bkgr( void *data, void *pos )
{
    int32_t bkgr;

    func_get_args( SET_ARG(bkgr), -1 );

    LOG( 3, "set background \"%s\"\n", (char *)data + bkgr );
    set_background( get_resource( res02, (char *)data + bkgr ) );

    return pos;
}

void *op_grp_hide_bkgr( void *data, void *pos )
{
    uint32_t time;

    time = stack_pop();

    LOG( 3, "hide background in %d ms\n", time );
    set_background( NULL );

    return pos;
}

void *op_grp_char_set( void *data, void *pos )
{
    int32_t sprite;
    int id; // or depth?
    sprite_info_t sprite_info;
    int effect_time;
    int effect_do;

    func_get_args( SET_ARG(id), SET_ARG(sprite), SET_ARG(sprite_info.x), SET_ARG(sprite_info.y),
                   NULL_ARG,
                   NULL_ARG,    // maybe effect enable
                   NULL_ARG,    // maybe initial X
                   NULL_ARG,    // maybe initial Y
                   NULL_ARG, NULL_ARG, NULL_ARG, NULL_ARG, NULL_ARG,
                   NULL_ARG,
                   SET_ARG(effect_time),    // seems like effect length
                   SET_ARG(effect_do),      // do effect or remember it?
                   -1 );

    sprite_info.img = get_resource( res02, (char *)data + sprite );
    LOG( 3, "set sprite at %d in %d ms, %d\n", id, effect_time, effect_do );
    if ( sprite_info.img )
        set_sprite( id, &sprite_info );
    else
    {
        LOG( 3, "error while getting sprite!\n");
        exit(1);
    }

    return pos;
}

void *op_grp_char_hide( void *data, void *pos )
{
    int id; // or depth?

    func_get_args( SET_ARG(id), -1 );

    LOG( 3, "hide char at %d\n", id );
    set_sprite( id, NULL );

    return pos;
}

