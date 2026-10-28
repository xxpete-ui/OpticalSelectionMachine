#ifndef SQLMAPPING_H
#define SQLMAPPING_H
#include <QSqlQuery>
#include <QSqlRecord>
#include <QMetaObject>
#include <QMetaProperty>

/**
 * 将当前查询结果行映射到指定的实体对象（值语义，返回新对象）
 */
template<typename T>
void mapToEntity(T& entity, const QSqlQuery& query)
{
    const QMetaObject* meta = entity.metaObject(); // 2. 获取元对象指针（调用 entity 的虚函数）
    QSqlRecord record = query.record();     // 3. 获取查询结果的记录描述（包含列名、类型等）

    for (int i = 0; i < record.count(); ++i)   // 4. 遍历每一列
    {
        QString colName = record.fieldName(i); // 5. 获取当前列的名称（通常与数据库列名一致）
        //int propIdx = meta->indexOfProperty(colName.toUtf8()); // 6. 查找实体类中是否存在同名的 Q_PROPERTY
        int propIdx = meta->indexOfProperty(colName.toStdString().c_str()); // 6. 查找实体类中是否存在同名的 Q_PROPERTY
        if (propIdx >= 0)                     // 7. 如果找到了匹配的属性
        {
            QMetaProperty prop = meta->property(propIdx); // 8. 获取该属性的元信息
            QVariant value = query.value(i);  // 9. 获取当前行的该列值（QVariant 自动处理类型转换）
            // 10. 将值写入实体对象的该属性
            //    注意：如果属性使用 MEMBER 绑定，必须保证成员变量可写；
            //    如果使用 WRITE setter，则会调用对应的 set 函数。
            prop.write(&entity, value);
        }
        // 若未找到匹配的属性，则忽略该列（可根据需要输出警告）
    }
}
#endif // SQLMAPPING_H
