#include "pyjstypeconv.h"
#include "pyjsobject.h"
#include <iostream>
v8::Local<v8::Value> PyToJs(PyObject *pyObject) {
    Nan::EscapableHandleScope scope;
    if (pyObject == nullptr) {
        return scope.Escape(Nan::Undefined());
    } else if (pyObject == Py_None) {
        return scope.Escape(Nan::Null());
    } else if (PyUnicode_CheckExact(pyObject)) {
        Py_ssize_t size;
        char *str = PyUnicode_AsUTF8AndSize(pyObject, &size);
        return scope.Escape(Nan::New(str, size).ToLocalChecked());
    } else if (PyBool_Check(pyObject)) {
        return scope.Escape(Nan::New<v8::Boolean>(pyObject == Py_True));
    } else if (PyFloat_CheckExact(pyObject)) {
        return scope.Escape(Nan::New<v8::Number>(PyFloat_AsDouble(pyObject)));
    } else if (PyList_CheckExact(pyObject)) {
        v8::Local<v8::Array> jsArr = Nan::New<v8::Array>();
        Py_ssize_t size = PyList_Size(pyObject);
        for (ssize_t i = 0; i < size; i++) {
            jsArr->Set(i, PyToJs(PyList_GetItem(pyObject, i)));
        }
        return scope.Escape(jsArr);
    } else if (PyDict_CheckExact(pyObject)) {
        v8::Local<v8::Object> jsObject = Nan::New<v8::Object>();
        PyObject *key, *value;
        Py_ssize_t pos = 0;
        while (PyDict_Next(pyObject, &pos, &key, &value)) {
            jsObject->Set(PyToJs(key), PyToJs(value));
        }
        return scope.Escape(jsObject);
    }
    return scope.Escape(Nan::Undefined());
}

// return new reference
PyObject *JsToPy(v8::Local<v8::Value> jsValue) {
    Nan::HandleScope scope;
    if (jsValue->IsObject()) {
        v8::Local<v8::Object> jsObject = jsValue->ToObject();
        if (PyjsObject::IsInstance(jsObject)) {
            // just unwrap and return
            return Nan::ObjectWrap::Unwrap<PyjsObject>(jsObject)->GetObject();
        }
    }
    if (jsValue->IsNull()) {
        Py_INCREF(Py_None);
        return Py_None;
    } else if (jsValue->IsString()) {
        v8::Local<v8::String> jsString = jsValue->ToString();
        return PyUnicode_FromStringAndSize(*static_cast<v8::String::Utf8Value>(jsString), jsString->Utf8Length());
    } else if (jsValue->IsTrue()) {
        Py_INCREF(Py_True);
        return Py_True;
    } else if (jsValue->IsFalse()) {
        Py_INCREF(Py_False);
        return Py_False;
    } else if (jsValue->IsNumber()) {
        return PyFloat_FromDouble(jsValue->NumberValue());
    } else if (jsValue->IsArray()) {
        v8::Local<v8::Array> jsArr = jsValue.As<v8::Array>();
        PyObject *pyArr = PyList_New(jsArr->Length());
        for (ssize_t i = 0; i < jsArr->Length(); i++) {
            int result = PyList_SetItem(pyArr, i, JsToPy(jsArr->Get(i)));
        }
        return pyArr;
    } else if (jsValue->IsObject()) { // must be at last
        v8::Local<v8::Object> jsObject = jsValue->ToObject();
        PyObject *pyDict = PyDict_New();
        v8::Local<v8::Array> props = Nan::GetOwnPropertyNames(jsObject).ToLocalChecked();
        for (ssize_t i = 0; i < props->Length(); i++) {
            v8::Local<v8::Value> jsKey = props->Get(i);
            v8::Local<v8::Value> jsValue = jsObject->Get(jsKey);
            PyObject *pyKey = JsToPy(jsKey);
            PyObject *pyValue = JsToPy(jsValue);
            int result = PyDict_SetItem(pyDict, pyKey, pyValue);
            Py_DECREF(pyKey);
            Py_DECREF(pyValue);
        }
        return pyDict;
    }
    return nullptr;
}
