#pragma once

typedef struct QTailQLink {
	void* tql_next;// �����ڵ�������
	struct QTailQLink* tql_prev;// ������˫������
}QTailQLink;

/*
* Tail queue definition. The union acts as poor man template, as if
* it were QTailQLink<type>
*/
// ��ʾ����ͷ
#define QTAILQ_HEAD(name,type)										\
union name {														\
	struct type* tqh_first;	/* first element */						\
	QTailQLink tqh_circ;	/* link for circular backwards list */	\
}

#define QTAILQ_HEAD_INITIALIZER(head)                                   \
        { .tqh_circ = { NULL, &(head).tqh_circ } }

// ��ʾ���е�Ԫ��
#define QTAILQ_ENTRY(type)                                              \
union {                                                                 \
        struct type *tqe_next;        /* next element */                \
        QTailQLink tqe_circ;          /* link for circular backwards list */ \
}


/*
 * Tail queue functions.
 */
#define QTAILQ_INIT(head) do {                                          \
        (head)->tqh_first = NULL;                                       \
        (head)->tqh_circ.tql_prev = &(head)->tqh_circ;                  \
} while (/*CONSTCOND*/0)


#define QTAILQ_INSERT_TAIL(head, elm, field) do {                       \
        (elm)->field.tqe_next = NULL;                                   \
        (elm)->field.tqe_circ.tql_prev = (head)->tqh_circ.tql_prev;     \
        (head)->tqh_circ.tql_prev->tql_next = (elm);                    \
        (head)->tqh_circ.tql_prev = &(elm)->field.tqe_circ;             \
} while (/*CONSTCOND*/0)

#define QTAILQ_FOREACH(var, head, field)                                \
        for ((var) = ((head)->tqh_first);                               \
                (var);                                                  \
                (var) = ((var)->field.tqe_next))