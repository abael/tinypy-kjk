
tp_obj tp_str(tp_vm *tp, tp_obj self) {
    int type = obj_type(self);
    if (type == TP_STRING) { return self; }
    if (type == TP_NUMBER) {
        tp_num v = self->number.val;
        if ((fabs(v)-fabs((long)v)) < 0.000001) { return tp_printf(tp,"%ld",(long)v); }
        return tp_printf(tp,"%f",v);
    } else if(type == TP_DICT) {
        return tp_printf(tp,"<dict 0x%x>",self->dict.val);
    } else if(type == TP_LIST) {
        return tp_printf(tp,"<list 0x%x>",self->list.val);
    } else if (type == TP_NONE) {
        return tp_string(tp, "None");
    } else if (type == TP_DATA) {
        return tp_printf(tp,"<data 0x%x>",self->data.val);
    } else if (type == TP_FNC) {
        return tp_printf(tp,"<fnc 0x%x>",self->fnc.val);
    }
    return tp_string(tp, "<?>");
}

int tp_bool(TP,tp_obj v) {
    switch(obj_type(v)) {
        case TP_NUMBER: return v->number.val != 0;
        case TP_NONE: return 0;
        case TP_STRING: return v->string.len != 0;
        case TP_LIST: return v->list.val->len != 0;
        case TP_DICT: return v->dict.val->len != 0;
        default: /* TP_FNC, TP_DATA */
            return 1;
    }
}

tp_obj tp_has(tp_vm *tp, tp_obj self, tp_obj k) {
    int type = obj_type(self);
    if (type == TP_DICT) {
        if (_tp_dict_find(tp,self->dict.val,k) != -1) { return True; }
        return False;
    } else if (type == TP_STRING && obj_type(k) == TP_STRING) {
        char *p = strstr(STR(self),STR(k));
        return tp_number(tp, p != 0);
    } else if (type == TP_LIST) {
        return tp_number(tp, _tp_list_find(tp,self->list.val,k)!=-1);
    }
    tp_raise(None,"tp_has(%s,%s)",STR(self),STR(k));
}

void tp_del(TP,tp_obj self, tp_obj k) {
    int type = obj_type(self);
    if (type == TP_DICT) {
        _tp_dict_del(tp,self->dict.val,k,"tp_del");
        return;
    }
    tp_raise(,"tp_del(%s,%s)",STR(self),STR(k));
}


tp_obj tp_iter(TP,tp_obj self, tp_obj k) {
    int type = obj_type(self);
    if (type == TP_LIST || type == TP_STRING) { return tp_get(tp,self,k); }
    if (type == TP_DICT && obj_type(k) == TP_NUMBER) {
        return self->dict.val->items[_tp_dict_next(tp,self->dict.val)].key;
    }
    tp_raise(None,"tp_iter(%s,%s)",STR(self),STR(k));
}

tp_obj tp_get(tp_vm *tp, tp_obj self, tp_obj k) {
    int type = obj_type(self);
    tp_obj r;
    if (type == TP_DICT) {
        return _tp_dict_get(tp,self->dict.val,k,"tp_get");
    } else if (type == TP_LIST) {
        if (obj_type(k) == TP_NUMBER) {
            int l = tp_len(tp,self)->number.val;
            int n = k->number.val;
            n = (n<0?l+n:n);
            return _tp_list_get(tp,self->list.val,n,"tp_get");
        } else if (obj_type(k) == TP_STRING) {
            if (strcmp("append",STR(k)) == 0) {
                return tp_method(tp,self,tp_append);
            } else if (strcmp("pop",STR(k)) == 0) {
                return tp_method(tp,self,tp_pop);
            } else if (strcmp("index",STR(k)) == 0) {
                return tp_method(tp,self,tp_index);
            } else if (strcmp("sort",STR(k)) == 0) {
                return tp_method(tp,self,tp_sort);
            } else if (strcmp("extend",STR(k)) == 0) {
                return tp_method(tp,self,tp_extend);
            } else if (strcmp("*",STR(k)) == 0) {
                tp_params_v(tp,1,self); 
                r = tp_copy(tp);
                self->list.val->len=0;
                return r;
            }
        } else if (obj_type(k) == TP_NONE) {
            return _tp_list_pop(tp,self->list.val,0,"tp_get");
        }
    } else if (type == TP_STRING) {
        if (obj_type(k) == TP_NUMBER) {
            int l = self->string.len;
            int n = k->number.val;
            n = (n<0?l+n:n);
            if (n >= 0 && n < l) { 
                return tp_string_n(tp, tp->chars[(unsigned char)self->string.val[n]],1); 
            }
        } else if (obj_type(k) == TP_STRING) {
            if (strcmp("join",STR(k)) == 0) {
                return tp_method(tp,self,tp_join);
            } else if (strcmp("split",STR(k)) == 0) {
                return tp_method(tp,self,tp_split);
            } else if (strcmp("index",STR(k)) == 0) {
                return tp_method(tp,self,tp_str_index);
            } else if (strcmp("strip",STR(k)) == 0) {
                return tp_method(tp,self,tp_strip);
            } else if (strcmp("replace",STR(k)) == 0) {
                return tp_method(tp,self,tp_replace);
            }
        }
    } else if (type == TP_DATA) {
        return self->data.meta->get(tp,self,k);
    }

    if (obj_type(k) == TP_LIST) {
        int a,b,l;
        tp_obj tmp;
        l = tp_len(tp,self)->number.val;
        tmp = tp_get(tp, k, tp_number(tp, 0));
        if (obj_type(tmp) == TP_NUMBER) { 
            a = tmp->number.val; 
        } else if (obj_type(tmp) == TP_NONE) { 
            a = 0; 
        } else { 
            tp_raise(None,"%s is not a number",STR(tmp)); 
        }
        tmp = tp_get(tp, k, tp_number(tp, 1));
        if (obj_type(tmp) == TP_NUMBER) { 
            b = tmp->number.val; 
        } else if (obj_type(tmp) == TP_NONE) { 
            b = l; 
        } else { 
            tp_raise(None,"%s is not a number",STR(tmp)); 
        }
        a = _tp_max(0,(a<0?l+a:a)); 
        b = _tp_min(l,(b<0?l+b:b));
        if (type == TP_LIST) {
            return tp_list_n(tp,b-a, &self->list.val->items[a]);
        } else if (type == TP_STRING) {
            tp_obj r = tp_string_t(tp, b-a);
            char *ptr = r->string.val;
            memcpy(ptr,self->string.val+a,b-a); 
            ptr[b-a]=0;
            return tp_track(tp,r);
        }
    }

    tp_raise(None,"tp_get(%s,%s)",STR(self),STR(k));
}

int tp_iget(TP,tp_obj *r, tp_obj self, tp_obj k) {
    if (obj_type(self) == TP_DICT) {
        int n = _tp_dict_find(tp, self->dict.val,k);
        if (n == -1) { 
            return 0; 
        }
        *r = self->dict.val->items[n].val;
        tp_grey(tp,*r);
        return 1;
    }
    if (obj_type(self) == TP_LIST && !self->list.val->len) { return 0; }
    *r = tp_get(tp,self,k); tp_grey(tp,*r);
    return 1;
}

void tp_set(TP,tp_obj self, tp_obj k, tp_obj v) {
    int type = obj_type(self);

    if (type == TP_DICT) {
        _tp_dict_set(tp, self->dict.val,k,v);
        return;
    } else if (type == TP_LIST) {
        if (obj_type(k) == TP_NUMBER) {
            _tp_list_set(tp, self->list.val, k->number.val,v,"tp_set");
            return;
        } else if (obj_type(k) == TP_NONE) {
            _tp_list_append(tp, self->list.val,v);
            return;
        } else if (obj_type(k) == TP_STRING) {
            if (strcmp("*", STR(k)) == 0) {
                tp_params_v(tp,2,self,v); 
                tp_extend(tp);
                return;
            }
        }
    } else if (type == TP_DATA) {
        self->data.meta->set(tp,self,k,v);
        return;
    }
    tp_raise(,"tp_set(%s,%s,%s)",STR(self),STR(k),STR(v));
}

tp_obj tp_add(tp_vm *tp, tp_obj a, tp_obj b) {
    if (obj_type(a) == TP_NUMBER && obj_type(a) == obj_type(b)) {
        return tp_number(tp, a->number.val + b->number.val);
    } else if (obj_type(a) == TP_STRING && obj_type(a) == obj_type(b)) {
        int al = a->string.len, bl = b->string.len;
        tp_obj r = tp_string_t(tp,al+bl);
        char *s = r->string.val;
        memcpy(s, a->string.val,al); 
        memcpy(s+al, b->string.val,bl);
        return tp_track(tp,r);
    } else if (obj_type(a) == TP_LIST && obj_type(a) == obj_type(b)) {
        tp_obj r;
        tp_params_v(tp,1,a); 
        r = tp_copy(tp);
        tp_params_v(tp,2,r,b); 
        tp_extend(tp);
        return r;
    }
    tp_raise(None,"tp_add(%s,%s)",STR(a),STR(b));
}

tp_obj tp_mul(tp_vm *tp, tp_obj a, tp_obj b) {
    if (obj_type(a) == TP_NUMBER && obj_type(a) == obj_type(b)) {
        return tp_number(tp, a->number.val * b->number.val);
    } else if (obj_type(a) == TP_STRING && obj_type(b) == TP_NUMBER) {
        int al = a->string.len; 
        int n = b->number.val;
        tp_obj r = tp_string_t(tp,al*n);
        char *s = r->string.val;
        int i; 
        for (i=0; i<n; i++) { 
            memcpy(s+al*i, a->string.val,al); 
        }
        return tp_track(tp,r);
    }
    tp_raise(None,"tp_mul(%s,%s)",STR(a),STR(b));
}

double _tp_len(tp_obj v) {
    int type = obj_type(v);
    if (type == TP_STRING) {
        return v->string.len;
    } else if (type == TP_DICT) {
        return v->dict.val->len;
    } else if (type == TP_LIST) {
        return v->list.val->len;
    }
    return -1.0;
}

tp_obj tp_len(tp_vm *tp, tp_obj self) {
    double len = _tp_len(self);
    if (-1.0 == len) {
        tp_raise(None, "tp_len(%s)", STR(self));
    }
    return tp_number(tp, len);
}

int tp_cmp(tp_vm *tp, tp_obj a, tp_obj b) {
    if (obj_type(a) != obj_type(b)) {
        return obj_type(a) - obj_type(b);
    }

    switch (obj_type(a)) {
        case TP_NONE: return 0;
        case TP_NUMBER: return _tp_sign(a->number.val - b->number.val);
        case TP_STRING: {
            int minlen = _tp_min(a->string.len, b->string.len);
            int v = memcmp(a->string.val, b->string.val, minlen);
            if (v == 0) { 
                v = a->string.len - b->string.len; 
            }
            return v;
        }
        case TP_LIST: {
            int n,v;
            int minlen = _tp_min(a->list.val->len,b->list.val->len);
            for (n=0; n < minlen; n++) {
                tp_obj aa = a->list.val->items[n]; 
                tp_obj bb = b->list.val->items[n];
                if (obj_type(aa) == TP_LIST && obj_type(bb) == TP_LIST) { 
                    v = aa->list.val - bb->list.val; 
                } else { 
                    v = tp_cmp(tp,aa,bb); 
                }
                if (v) { 
                    return v; 
                } 
            }
            return a->list.val->len - b->list.val->len;
        }
        case TP_DICT: return a->dict.val - b->dict.val;
        case TP_FNC: return a->fnc.val - b->fnc.val;
        case TP_DATA: return (char*)a->data.val - (char*)b->data.val;
    }
    tp_raise(0,"tp_cmp(%s,%s)",STR(a),STR(b));
}

#define TP_OP(name,expr) \
    tp_obj name(tp_vm *tp, tp_obj _a, tp_obj _b) { \
    if (obj_type(_a) == TP_NUMBER && obj_type(_a) == obj_type(_b)) { \
        tp_num a = _a->number.val; \
        tp_num b = _b->number.val; \
        return tp_number(tp, expr); \
    } \
    tp_raise(None,"%s(%s,%s)",#name,STR(_a),STR(_b)); \
}

TP_OP(tp_and,((long)a)&((long)b));
TP_OP(tp_or,((long)a)|((long)b));
TP_OP(tp_mod,((long)a)%((long)b));
TP_OP(tp_lsh,((long)a)<<((long)b));
TP_OP(tp_rsh,((long)a)>>((long)b));
TP_OP(tp_sub,a-b);
TP_OP(tp_div,a/b);
TP_OP(tp_pow,pow(a,b));
