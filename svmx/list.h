#pragma once
#include "pch.h"


#define HLIST_HEAD_INIT {.first = NULL }
#define HLIST_HEAD(name) struct hlist_head name = {.first = NULL}
#define INIT_HLIST_HEAD(ptr) ((ptr)->first = NULL)
static inline void INIT_HLIST_NODE(struct hlist_node* h)
{
	h->next = NULL;
	h->pprev = NULL;
}

// �жϽ���Ƿ�һ����hash����
static inline int hlist_unhashed(const struct hlist_node* h)
{
	return !h->pprev;
}

static inline void __hlist_del(struct hlist_node* n)
{
	// ��ȡָ���ɾ��������һ����ͨ����ָ��
	struct hlist_node* next = n->next;
	// ��ȡ��ɾ���ڵ��pprev��
	struct hlist_node** pprev = n->pprev;

	// ����ָ����һ���ڵ�
	*pprev = next;
	if (next) // ����ýڵ㲻Ϊ��
		next->pprev = pprev;
}

static inline void hlist_del(struct hlist_node* n) {
	__hlist_del(n);
	n->next = NULL;
	n->pprev = NULL;
}

#define hlist_entry(ptr,type,member) CONTAINING_RECORD(ptr,type,member)

#define hlist_entry_safe(ptr,type,member) \
	(  \
	   ptr ? hlist_entry(ptr,type, member) : NULL \
	)

// �����n����ͷ���h֮��
static inline void hlist_add_head(struct hlist_node* n, struct hlist_head* h)
{
	struct hlist_node* first = h->first;
	// ָ����һ������NULL
	n->next = first;
	// firstָ��ǿգ����̽���pprevָ��ǰ������next��ַ
	if (first)
		first->pprev = &n->next;
	h->first = n;
	n->pprev = &h->first;
}

// �����n����next����ǰ��
static inline void hlist_add_before(struct hlist_node* n,
	struct hlist_node* next) {
	n->pprev = next->pprev;
	n->next = next;
	next->pprev = &n->next;
	*(n->pprev) = n;
}

/*
* add a new entry after the one specified
* @n: new entry to be added
* @prev: hlist node to add it after, which must be non-NULL
*/
static inline void hlist_add_behind(struct hlist_node* n,
	struct hlist_node* prev) {
	n->next = prev->next;
	prev->next = n;
	n->pprev = &prev->next;

	if (n->next)
		n->next->pprev = &n->next;
}

/*
* Is the specified hlist_head structure an empty hlist ?
* @h: Sturcture to check.
*/ 
static inline int hlist_empty(const struct hlist_head* h)
{
	return !h->first;
}

static inline void hlist_del_init(struct hlist_node* n)
{
	if (!hlist_unhashed(n)) {
		__hlist_del(n);
		INIT_HLIST_NODE(n);
	}
}
