#pragma once

#include "RLibX/JPEG.h"

#include <stdint.h>

namespace PDF {

class Document;
class Resources;
class Page;
class PageRef;
class ImageRegistry;
class FontRegistry;


class ObjectId
{
public:
    ObjectId() : m_value(0) {}
    ObjectId(uint32_t value) : m_value(value) {}

    uint32_t value() const { return m_value; }

private:
    uint32_t m_value;
};

class TypedObjectId
{
public:
    TypedObjectId() : m_objectId() {}

    ObjectId objectId() const { return m_objectId; }
    uint32_t value() const { return m_objectId.value(); }

protected:
    TypedObjectId(ObjectId objectId) : m_objectId(objectId) {}

private:
    ObjectId m_objectId;
};

class PagesRootObjectId : public TypedObjectId
{
public:
    PagesRootObjectId() {}

private:
    friend class Document;

    PagesRootObjectId(ObjectId objectId) : TypedObjectId(objectId) {}
};

class CatalogObjectId : public TypedObjectId
{
public:
    CatalogObjectId() {}

private:
    friend class Document;

    CatalogObjectId(ObjectId objectId) : TypedObjectId(objectId) {}
};

class ResourcesObjectId : public TypedObjectId
{
public:
    ResourcesObjectId() {}

private:
    friend class Resources;

    ResourcesObjectId(ObjectId objectId) : TypedObjectId(objectId) {}
};

class PageObjectId : public TypedObjectId
{
public:
    PageObjectId() {}

private:
    friend class PageRef;
    friend class Page;

    PageObjectId(ObjectId objectId) : TypedObjectId(objectId) {}
};

class ContentsObjectId : public TypedObjectId
{
public:
    ContentsObjectId() {}

private:
    friend class Page;

    ContentsObjectId(ObjectId objectId) : TypedObjectId(objectId) {}
};

class LengthObjectId : public TypedObjectId
{
public:
    LengthObjectId() {}

private:
    friend class Page;

    LengthObjectId(ObjectId objectId) : TypedObjectId(objectId) {}
};

} // namespace PDF
