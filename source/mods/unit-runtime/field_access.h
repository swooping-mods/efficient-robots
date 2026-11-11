#ifndef MOON_UNIT_FIELD_ACCESS_H
#define MOON_UNIT_FIELD_ACCESS_H
/* Current IL2CPP references use the object itself as field_set_value's last
 * argument. Scalar fields instead use an address. Read back before publishing
 * a containing record to a game collection. */
typedef void (*MUFieldSet)(void *,void *,void *);
typedef void (*MUFieldGet)(void *,void *,void *);
static inline int mur_reference(void *object,void *field,void *value,MUFieldSet set,MUFieldGet get){
    if(!object||!field||!set||!get)return 0;
    set(object,field,value);void *observed=0;get(object,field,&observed);return observed==value;
}
#endif
