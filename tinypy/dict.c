int tp_lua_hash(void *v,int l) {
    int i,step = (l>>5)+1;
    int h = l + (l >= 4?*(int*)v:0);
    for (i=l; i>=step; i-=step) {
        h = h^((h<<5)+(h>>2)+((unsigned char *)v)[i-1]);
    }
    return h;
}
void _tp_dict_free(_tp_dict *self) {
    tp_free(self->items);
    tp_free(self);
}

/* void _tp_dict_reset(_tp_dict *self) {
       memset(self->items,0,self->alloc*sizeof(tp_item));
       self->len = 0;
       self->used = 0;
       self->cur = 0;
   }*/

int tp_hash(tp_vm *tp, tp_obj v) {
    switch (obj_type(v)) {
        case TP_NONE: return 0;
        case TP_NUMBER: 
            return tp_lua_hash(&tp_number_val(v), sizeof(tp_num));
        case TP_STRING: 
            return tp_lua_hash(tp_str_val(v), tp_str_len(v));
        case TP_DICT: 
            return tp_lua_hash(&tp_dict_val(v), sizeof(void*));
        case TP_LIST: {
            int r = tp_list_val(v)->len;
            int n; 
            for (n=0; n < tp_list_val(v)->len; n++) {
                tp_obj vv = tp_list_val(v)->items[n];
                if (obj_type(vv) == TP_LIST)
                    r += tp_lua_hash(&tp_list_val(vv), sizeof(void*));
                else
                    r += tp_hash(tp, tp_list_val(v)->items[n]);
            } 
            return r;
        }
        case TP_FNC: return tp_lua_hash(&tp_fnc_val(v), sizeof(void*));
        case TP_DATA: return tp_lua_hash(&tp_data_val(v), sizeof(void*));
    }
    tp_raise(0,"tp_hash(%s)",STR(v));
}

void _tp_dict_hash_set(TP,_tp_dict *self, int hash, tp_obj k, tp_obj v) {
    tp_item item;
    int i,idx = hash & self->mask;
    for (i=idx; i < idx+self->alloc; i++) {
        int n = i&self->mask;
        if (self->items[n].used > 0) { continue; }
        if (self->items[n].used == 0) { self->used += 1; }
        item.used = 1;
        item.hash = hash;
        item.key = k;
        item.val = v;
        self->items[n] = item;
        self->len += 1;
        return;
    }
    tp_raise(,"_tp_dict_hash_set(%d,%d,%s,%s)",self,hash,STR(k),STR(v));
}

void _tp_dict_tp_realloc(TP,_tp_dict *self,int len) {
    tp_item *items = self->items;
    int i,alloc = self->alloc;
    len = _tp_max(8,len);

    self->items = tp_malloc(len*sizeof(tp_item));
    self->alloc = len; self->mask = len-1;
    self->len = 0; self->used = 0;

    for (i=0; i<alloc; i++) {
        if (items[i].used != 1) { continue; }
        _tp_dict_hash_set(tp,self,items[i].hash,items[i].key,items[i].val);
    }
    tp_free(items);
}

int _tp_dict_hash_find(tp_vm *tp, _tp_dict *self, int hash, tp_obj k) {
    int i,idx = hash & self->mask;
    for (i=idx; i<idx+self->alloc; i++) {
        int n = i&self->mask;
        if (self->items[n].used == 0) { 
            break; 
        }
        if (self->items[n].used < 0) { 
            continue; 
        }
        if (self->items[n].hash != hash) { 
            continue; 
        }
        if (tp_cmp(tp, self->items[n].key, k) != 0) { 
            continue; 
        }
        return n;
    }
    return -1;
}

int _tp_dict_find(tp_vm *tp, _tp_dict *self, tp_obj k) {
    return _tp_dict_hash_find(tp, self, tp_hash(tp,k), k);
}

void _tp_dict_setx(TP,_tp_dict *self,tp_obj k, tp_obj v) {
    int hash = tp_hash(tp,k); int n = _tp_dict_hash_find(tp,self,hash,k);
    if (n == -1) {
        if (self->len >= (self->alloc/2)) {
            _tp_dict_tp_realloc(tp,self,self->alloc*2);
        } else if (self->used >= (self->alloc*3/4)) {
            _tp_dict_tp_realloc(tp,self,self->alloc);
        }
        _tp_dict_hash_set(tp,self,hash,k,v);
    } else {
        self->items[n].val = v;
    }
}

void _tp_dict_set(TP,_tp_dict *self,tp_obj k, tp_obj v) {
    _tp_dict_setx(tp,self,k,v);
    tp_grey(tp,k); tp_grey(tp,v);
}

tp_obj _tp_dict_get(TP,_tp_dict *self,tp_obj k, char *error) {
    int n = _tp_dict_find(tp,self,k);
    if (n < 0) {
        tp_raise(None,"%s: KeyError: %s\n",error,STR(k));
    }
    return self->items[n].val;
}

void _tp_dict_del(tp_vm *tp, _tp_dict *self, tp_obj k, char *error) {
    int n = _tp_dict_find(tp, self, k);
    if (n < 0) { 
#if 0
        /* TODO figure out this happens */
        tp_raise(, "%s: KeyError: '%s'\n", error, STR(k)); 
#else
        return;
#endif
    }
    self->items[n].used = -1;
    self->len -= 1;
}

_tp_dict *_tp_dict_new(void) {
    _tp_dict *self = tp_malloc(sizeof(_tp_dict));
    return self;
}

tp_obj _tp_dict_copy(tp_vm *tp, tp_obj rr) {
    tp_obj obj = obj_alloc(TP_DICT);
    _tp_dict *o = tp_dict_val(rr);
    _tp_dict *r = _tp_dict_new(); 
    *r = *o;
    r->items = tp_malloc(sizeof(tp_item)*o->alloc);
    memcpy(r->items, o->items, sizeof(tp_item)*o->alloc);
    tp_dict_set_val(obj, r);
    return tp_track(tp, obj);
}

int _tp_dict_next(TP,_tp_dict *self) {
    if (!self->len) { 
        tp_raise(0,"_tp_dict_next(...)",0); 
    }
    while (1) {
        self->cur = ((self->cur + 1) & self->mask);
        if (self->items[self->cur].used > 0) {
            return self->cur;
        }
    }
}

tp_obj tp_merge(TP) {
    tp_obj self = TP_OBJ();
    tp_obj v = TP_OBJ();
    int i; for (i=0; i<tp_dict_val(v)->len; i++) {
        int n = _tp_dict_next(tp, tp_dict_val(v));
        _tp_dict_set(tp, tp_dict_val(self),
            tp_dict_val(v)->items[n].key, tp_dict_val(v)->items[n].val);
    }
    return None;
}

tp_obj tp_dict(tp_vm *tp) {
    tp_obj r = obj_alloc(TP_DICT);
    tp_dict_set_val(r, _tp_dict_new());
    return tp ? tp_track(tp,r) : r;
}

tp_obj tp_dict_n(tp_vm *tp, int n, tp_obj* argv) {
    tp_obj r = tp_dict(tp);
    int i;
    for (i=0; i<n; i++) {
        tp_set(tp,r,argv[i*2],argv[i*2+1]);
    }
    return r;
}

